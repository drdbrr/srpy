#include <cstddef>
#include <atomic>
#include <new>

class alignas(2*alignof(int)) GuardedIndex
{
public:
    constexpr GuardedIndex() = default;
    constexpr GuardedIndex(int aIndex) : mIndex(aIndex) {}

    constexpr int Get_Index() const { return mIndex; }
    constexpr void Set_Index(int aIndex) { mIndex = aIndex; ++mGuardCount; }

private:
    int mIndex = 0;
    unsigned int mGuardCount = 0; //Unsigned so rollover is defined
};



template <typename DataType, bool Awaitable = false>
class Pool
{
    //...
private:

    //Get the cache line size (typically 64 bytes)
    static constexpr auto sMemberAlign =
        std::hardware_destructive_interference_size;

    //Modified frequently during operations:
    alignas(sMemberAlign) std::atomic<int> mSize = 0;
    alignas(sMemberAlign) GuardedIndex mHeadNodeIndex;

    //Not modified post-allocation:
    alignas(sMemberAlign) std::byte* mStorage = nullptr; //Object memory
    int* mFreeList = nullptr;
    int mCapacity = 0;
}

template <typename AllocatorType>
void Pool::Allocate(AllocatorType& aAllocator, int aCapacity)
{
    //Allocate the object memory
    static constexpr auto sDataAlign = std::max(alignof(DataType),
        sMemberAlign);
    mStorage = aAllocator.Allocate(aCapacity * sizeof(DataType), sDataAlign);
    mCapacity = aCapacity;

    //Allocate the free list memory
    static constexpr auto sListAlign = std::max(alignof(int), sMemberAlign);
    auto cFreeListMemory = aAllocator.Allocate(aCapacity * sizeof(int),
        sListAlign);
    mFreeList = reinterpret_cast<int*>(cFreeListMemory);

    //Initialize free list
    for (int ci = 0 ; ci < (aCapacity - 1); ++ci)
        new (&mFreeList[ci]) = int(ci + 1);
    new (&mFreeList[aCapacity - 1]) = int(-1);

    //Publish free list head node.
    mHeadNodeIndex.Set_Index(0);
}

template <typename AllocatorType>
void Pool::Free(AllocatorType& aAllocator)
{
    Assert(empty(), "Objects not destroyed!\n");

    //Note: no destruction of indices are needed: is trivial type
    aAllocator.Free(reinterpret_cast<std::byte*>(mFreeList));
    mFreeList = nullptr;
    aAllocator.Free(mStorage);
    mStorage = nullptr;

    mCapacity = 0;
}

int Pool::size() const
{
    return mSize.load(std::memory_order::relaxed);
}

bool Pool::empty() const
{
    return (size() == 0);
}

template <typename... ArgumentTypes>
std::pair<bool, int> Pool::Emplace(ArgumentTypes&&... aArguments)
{
    //Get initial value
    auto cHead = std::atomic_ref(mHeadNodeIndex);
    GuardedIndex cHeadEntry = cHead.load(std::memory_order::relaxed);

    //Loop until we reserve the head node for our object
    int cHeadIndex;
    GuardedIndex cNextSlotEntry;
    do
    {
        //Bail if we're full
        cHeadIndex = cHeadEntry.Get_Index();
        if (cHeadIndex == -1)
            return { false, cFirstSlot };

        //Find the next slot in the free list
        auto cListNode = std::atomic_ref(mFreeList[cFirstSlot]);
        auto cNextSlotIndex = cListNode.load(std::memory_order::relaxed);

        //Set next index and guard against ABA
        cNextSlotEntry = cHeadEntry;
        cNextSlotEntry.Set_Index(cNextSlotIndex);

        //Set new value for head index
    }
    while(!cHead.compare_exchange_weak(cHeadEntry, cNextSlotEntry,
        std::memory_order::acquire, std::memory_order::relaxed));

    //We have exclusive access to this slot. Create new object
    std::byte* cAddress = mStorage + cFirstSlot * sizeof(DataType);
    new (cAddress) DataType(std::forward<ArgumentTypes>(aArguments)...);

    //Update the size and return
    mSize.fetch_add(1, std::memory_order::relaxed);
    return { true, cHeadIndex };
}

DataType& Pool::operator[](int aIndex)
{
    //Non-const method calls the const method
    const auto& cConstThis = *const_cast<const Pool*>(this);
    return const_cast<DataType&>(cConstThis[aIndex]);
}

const DataType& Pool::operator[](int aIndex) const
{
    const std::byte* cAddress = mStorage + aIndex * sizeof(DataType);
    return *std::launder(reinterpret_cast<const DataType*>(cAddress));
}

void Pool::Erase(int aIndex)
{
    //Destroy the object
    (*this)[aIndex].~DataType();

    //Add this index to the front of the list
    auto cHead = std::atomic_ref(mHeadNodeIndex);
    GuardedIndex cHeadEntry = cHead.load(std::memory_order::relaxed);
    GuardedIndex cNewHeadEntry;
    do {
        //Store the current head node in our free list slot
        auto cListNode = std::atomic_ref(mFreeList[aIndex]);
        cListNode.store(cHeadEntry.Get_Index(), std::memory_order::relaxed);

        //Set our index and guard against ABA
        cNewHeadEntry = cHeadEntry;
        cNewHeadEntry.Set_Index(aIndex);

        //Set new value
    } while (!cHead.compare_exchange_weak(cHeadEntry, cNewHeadEntry,
        std::memory_order::release, std::memory_order::relaxed))

    //Update the size
    [[maybe_unused]] auto cPriorSize = mSize.fetch_sub(1,
        std::memory_order::relaxed);

    //If awaitable, inform any waiters that a slot has freed up
    if constexpr (Awaitable)
    {
        if (cPriorSize == mCapacity)
            mSize.notify_all();
    }
}

template <typename... ArgumentTypes>
int Pool::Emplace_Await(ArgumentTypes&&... aArguments) requires (Awaitable)
{
    while(true)
    {
        auto [cCreated, cIndex] =
            Emplace(std::forward<ArgumentTypes>(aArguments)...);

        if(cCreated)
            return cIndex;

        //The pool was full, wait until we're notified
        mSize.wait(mCapacity, std::memory_order::relaxed);
    }
}

