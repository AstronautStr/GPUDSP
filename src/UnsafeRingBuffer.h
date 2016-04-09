//
//  UnsafeRingBuffer.h
//  AnotherSandboxProject
//
//  Created by Ilya Solovyov on 08.04.16.
//
//

#ifndef UnsafeRingBuffer_h
#define UnsafeRingBuffer_h

template <typename T> class UnsafeRingBufferT
{
public:
    //! Constructs a UnsafeRingBufferT with size = 0
    UnsafeRingBufferT() : mData( nullptr ), mAllocatedSize( 0 ), mWriteIndex( 0 ), mReadIndex( 0 ) {}
    //! Constructs a UnsafeRingBufferT with \a count maximum elements.
    UnsafeRingBufferT( size_t count ) : mAllocatedSize( 0 )
    {
        resize( count );
    }
    
    UnsafeRingBufferT( UnsafeRingBufferT &&other )
    : mData( other.mData ), mAllocatedSize( other.mAllocatedSize ), mWriteIndex( 0 ), mReadIndex( 0 )
    {
        other.mData = nullptr;
        other.mAllocatedSize = 0;
    }
    
    ~UnsafeRingBufferT()
    {
        if( mData )
            free( mData );
    }
    //! Resizes the container to contain \a count maximum elements. Invalidates the internal buffer and resets read / write indices to 0. \note Must be synchronized with both read and write threads.
    void resize( size_t count )
    {
        size_t allocatedSize = count + 1; // one bin is used to distinguish between the read and write indices when full.
        
        if( mAllocatedSize )
            mData = (T *)::realloc( mData, allocatedSize * sizeof( T ) );
        else
            mData = (T *)::calloc( allocatedSize, sizeof( T ) );
        
        CI_ASSERT( mData );
        
        mAllocatedSize = allocatedSize;
        clear();
    }
    //! Invalidates the internal buffer and resets read / write indices to 0. \note Must be synchronized with both read and write threads.
    void clear()
    {
        mWriteIndex = 0;
        mReadIndex = 0;
    }
    //! Returns the maximum number of elements.
    size_t getSize() const
    {
        return mAllocatedSize - 1;
    }
    //! Returns the number of elements available for wrtiing. \note Only safe to call from the write thread.
    size_t getAvailableWrite() const
    {
        return getAvailableWrite( mWriteIndex, mReadIndex );
    }
    //! Returns the number of elements available for wrtiing. \note Only safe to call from the read thread.
    size_t getAvailableRead() const
    {
        return getAvailableRead( mWriteIndex, mReadIndex );
    }
    
    //! \brief Writes \a count elements into the internal buffer from \a array. \return `true` if all elements were successfully written, or `false` otherwise.
    //!
    //! \note only safe to call from the write thread.
    //! TODO: consider renaming this to writeAll / readAll, and having generic read / write that just does as much as it can
    bool write( const T *array, size_t count )
    {
        const size_t writeIndex = mWriteIndex.load( std::memory_order_relaxed );
        const size_t readIndex = mReadIndex.load( std::memory_order_acquire );
        
        if( count > getAvailableWrite( writeIndex, readIndex ) )
            return false;
        
        size_t writeIndexAfter = writeIndex + count;
        
        if( writeIndex + count > mAllocatedSize ) {
            size_t countA = mAllocatedSize - writeIndex;
            size_t countB = count - countA;
            
            std::memcpy( mData + writeIndex, array, countA * sizeof( T ) );
            std::memcpy( mData, array + countA, countB * sizeof( T ) );
            writeIndexAfter -= mAllocatedSize;
        }
        else {
            std::memcpy( mData + writeIndex, array, count * sizeof( T ) );
            if( writeIndexAfter == mAllocatedSize )
                writeIndexAfter = 0;
        }
        
        mWriteIndex.store( writeIndexAfter, std::memory_order_release );
        return true;
    }
    //! \brief Reads \a count elements from the internal buffer into \a array.  \return `true` if all elements were successfully read, or `false` otherwise.
    //!
    //! \note only safe to call from the read thread.
    bool read( T *array, size_t count )
    {
        const size_t writeIndex = mWriteIndex.load( std::memory_order_acquire );
        const size_t readIndex = mReadIndex.load( std::memory_order_relaxed );
        
        if( count > getAvailableRead( writeIndex, readIndex ) )
            return false;
        
        size_t readIndexAfter = readIndex + count;
        
        if( readIndex + count > mAllocatedSize ) {
            size_t countA = mAllocatedSize - readIndex;
            size_t countB = count - countA;
            
            std::memcpy( array, mData + readIndex, countA * sizeof( T ) );
            std::memcpy( array + countA, mData, countB * sizeof( T ) );
            
            readIndexAfter -= mAllocatedSize;
        }
        else {
            std::memcpy( array, mData + readIndex, count * sizeof( T ) );
            if( readIndexAfter == mAllocatedSize )
                readIndexAfter = 0;
        }
        
        mReadIndex.store( readIndexAfter, std::memory_order_release );
        return true;
    }
    
    void getUnsafeDataWritePointer(size_t count, T*& firstPart, T*& secondPart, size_t& firstLength, size_t& secondLength)
    {
        firstPart = nullptr;
        secondPart = nullptr;
        firstLength = 0;
        secondLength = 0;
        
        const size_t writeIndex = mWriteIndex.load( std::memory_order_relaxed );
        const size_t readIndex = mReadIndex.load( std::memory_order_acquire );
        
        if( count > getAvailableWrite( writeIndex, readIndex ) )
        {
            return;
        }
        
        size_t writeIndexAfter = writeIndex + count;
        
        if( writeIndex + count > mAllocatedSize )
        {
            size_t countA = mAllocatedSize - writeIndex;
            size_t countB = count - countA;
            
            firstPart = mData + writeIndex;
            firstLength = countA;
            secondPart = mData;
            secondLength = countB;
            
            writeIndexAfter -= mAllocatedSize;
        }
        else
        {
            firstPart = mData + writeIndex;
            firstLength = count;
            
            if( writeIndexAfter == mAllocatedSize )
                writeIndexAfter = 0;
        }
        
        mWriteIndex.store( writeIndexAfter, std::memory_order_release );
    }
    
private:
    size_t getAvailableWrite( size_t writeIndex, size_t readIndex ) const
    {
        size_t result = readIndex - writeIndex - 1;
        if( writeIndex >= readIndex )
            result += mAllocatedSize;
        
        return result;
    }
    
    size_t getAvailableRead( size_t writeIndex, size_t readIndex ) const
    {
        if( writeIndex >= readIndex )
            return writeIndex - readIndex;
        
        return writeIndex + mAllocatedSize - readIndex;
    }
    
    
    T						*mData;
    size_t					mAllocatedSize;
    std::atomic<size_t>		mWriteIndex, mReadIndex;
};


#endif /* UnsafeRingBuffer_h */
