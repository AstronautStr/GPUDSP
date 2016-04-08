#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"
#include "cinder/audio/dsp/Dsp.h"
#include "cinder/app/cocoa/PlatformCocoa.h"

#include <OpenCL/OpenCL.h>

using namespace ci;
using namespace ci::app;
using namespace ci::audio;
using namespace std;

#define LOGENABLED 0
#define UNSAFEBUFFER 1

typedef GLfloat                     SampleValueType;

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


#if UNSAFEBUFFER
typedef UnsafeRingBufferT<SampleValueType> RingBuffer;
#else
typedef ci::audio::dsp::RingBufferT<SampleValueType> RingBuffer;
#endif


class ExternalDSPNode : public GenNode
{
protected:
    RingBuffer* _ringBuffer;

public:
    ExternalDSPNode(RingBuffer* externalRingBuffer)
    {
        _ringBuffer = externalRingBuffer;
    }
    
    void process(audio::Buffer* buffer)
    {
        if (_ringBuffer)
        {
            float* data = buffer->getData();
#if LOGENABLED
            if (
#endif
            _ringBuffer->read(data, buffer->getNumFrames())
#if LOGENABLED
                == false)
                std::cerr << "[AudioThread]: BUFFERSKIP" << std::endl;
#else
            ;
#endif
            
        }
    }
};
typedef std::shared_ptr<class ExternalDSPNode> ExternalDSPNodeRef;


class GPUDSPController
{
public:
    RingBuffer RingBuffer;
    
private:
    GLuint _nodesVBO;
    GLuint _nodesVAO;
    GLuint _waveTableBuffer;
    GLuint _waveTableTex;
    GLuint _DSPFeedbackBuffer;
    SampleValueType* _feedbackData;
    
    GLuint _DSPShader;
    GLuint _DSPProgram;
    GLuint _samplesProcessedUniformLoc;
    GLuint _waveTableSamplerLoc;
    
    GLuint _samplesTex;
    
    size_t _samplesProcessed;
    size_t _sampleRate;
    
    std::string readAllText(std::string const& path)
    {
        std::ifstream ifstr(path);
        if (!ifstr)
            return std::string();
        std::stringstream text;
        ifstr >> text.rdbuf();
        return text.str();
    }
    
    typedef std::basic_string<GLchar>   GLstring;
    
    GLstring lastError(GLuint shader)
    {
        GLint logLen{ 0 };
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        std::vector<GLchar> log;
        log.resize(logLen, '\0');
        glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
        
        return GLstring(std::begin(log), std::end(log));
    }
    
    void _prepareSoundFeedback()
    {
        _prepareSoundFeedbackProgram();
        _prepareSoundFeedbackBuffers();
        _prepareSoundFeedbackVertexArray();
    }
    void _prepareSoundFeedbackProgram()
    {
        std::string vertexShaderSrcString = readAllText(PlatformCocoa::get()->getResourcePath("GPUDSP.vert").string());
        GLchar const* const vertexShaderSrc = vertexShaderSrcString.c_str();
        
        _DSPShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(_DSPShader, 1, &vertexShaderSrc, nullptr);
        glCompileShader(_DSPShader);
        
        std::cerr << lastError(_DSPShader) << std::endl;
        
        _DSPProgram = glCreateProgram();
        glAttachShader(_DSPProgram, _DSPShader);
        
        const GLchar* feedbackVaryings[] = { "sampleValue" };
        glTransformFeedbackVaryings(_DSPProgram, 1, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);
        
        glLinkProgram(_DSPProgram);
        std::cerr << lastError(_DSPProgram) << std::endl;
        
        GLint sampleRateLoc = glGetUniformLocation(_DSPProgram, "sampleRate");
        glProgramUniform1i(_DSPProgram, sampleRateLoc, _sampleRate);
        
        _samplesProcessedUniformLoc = glGetUniformLocation(_DSPProgram, "samplesProcessed");
        _waveTableSamplerLoc = glGetUniformLocation(_DSPProgram, "waveTable");
    }
    void _prepareSoundFeedbackBuffers()
    {
        size_t nodesCount = getBufferSize();
        size_t _samplesBytesSize = nodesCount * sizeof(SampleValueType);
        _feedbackData = new SampleValueType[nodesCount];
        
        GLint* sampleIndexData = new GLint[nodesCount];
        int sampleIndexDataBytesSize = nodesCount * sizeof(GLint);
        for (size_t i = 0; i < nodesCount; ++i)
        {
            sampleIndexData[i] = i;
            _feedbackData[i] = 0;
        }
        glGenBuffers(1, &_DSPFeedbackBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, _DSPFeedbackBuffer);
        glBufferData(GL_ARRAY_BUFFER, _samplesBytesSize, _feedbackData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glGenBuffers(1, &_nodesVBO);
        glBindBuffer(GL_ARRAY_BUFFER, _nodesVBO);
        glBufferData(GL_ARRAY_BUFFER, sampleIndexDataBytesSize, sampleIndexData, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        
        SampleValueType* waveTable = new SampleValueType[_sampleRate];
        size_t waveTableBytesSize = _sampleRate * sizeof(SampleValueType);
        for (size_t i = 0; i < _sampleRate; ++i)
        {
            waveTable[i] = (SampleValueType)cos(2 * M_PI * (double)i / (double)_sampleRate);
        }
        
        glGenBuffers(1, &_waveTableBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, _waveTableBuffer);
        glBufferData(GL_ARRAY_BUFFER, waveTableBytesSize, waveTable, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        delete [] waveTable;
        
        glGenTextures(1, &_waveTableTex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, _waveTableTex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, _waveTableBuffer);
        glBindTexture(GL_TEXTURE_BUFFER, 0);

    }
    void _prepareSoundFeedbackVertexArray()
    {
        glGenVertexArrays(1, &_nodesVAO);
        glBindVertexArray(_nodesVAO);
        
        glBindBuffer(GL_ARRAY_BUFFER, _nodesVBO);
        GLint samplePositionAttr = glGetAttribLocation(_DSPProgram, "samplePosition");
        glVertexAttribIPointer(samplePositionAttr, 1, GL_INT, 0, NULL);
        glEnableVertexAttribArray(samplePositionAttr);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    
    void _generateNodes()
    {
        _prepareSoundFeedbackProgram();
        _prepareSoundFeedbackBuffers();
        _prepareSoundFeedbackVertexArray();
    }
    
public:
    GPUDSPController(size_t sampleRate, size_t bufferSize) :
    RingBuffer(bufferSize)
    {
        _samplesProcessed = 0;
        _sampleRate = sampleRate;
        _generateNodes();
    }
    
    ~GPUDSPController()
    {
        delete [] _feedbackData;
        
        glDeleteProgram(_DSPProgram);
        glDeleteShader(_DSPShader);
        
        glDeleteBuffers(1, &_nodesVBO);
        glDeleteBuffers(1, &_DSPFeedbackBuffer);
        glDeleteBuffers(1, &_waveTableBuffer);
        glDeleteTextures(1, &_waveTableTex);
        
        glDeleteVertexArrays(1, &_nodesVAO);
    }
    
    size_t getBufferSize()
    {
        return RingBuffer.getSize();
    }
    
    void generateSamples()
    {
        size_t toWrite = RingBuffer.getAvailableWrite();
        
        if (toWrite <= 0)
            return;
            
        glEnable(GL_RASTERIZER_DISCARD);
        glUseProgram(_DSPProgram);
        
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, _DSPFeedbackBuffer);
        glBindVertexArray(_nodesVAO);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, _waveTableTex);
        glUniform1i(_waveTableSamplerLoc, 0);
        
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, toWrite);
        glEndTransformFeedback();
        
        glFlush();
        
        glBindVertexArray(0);
        glDisable(GL_RASTERIZER_DISCARD);
        glUseProgram(0);
        
        // recieve processed data from feedback
#if UNSAFEBUFFER
        SampleValueType* firstPart = nullptr;
        SampleValueType* secondPart = nullptr;
        size_t firstLength = 0;
        size_t secondLength = 0;
        
        RingBuffer.getUnsafeDataWritePointer(toWrite, firstPart, secondPart, firstLength, secondLength);
        if (firstPart != nullptr)
        {
            glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, firstLength * sizeof(SampleValueType), firstPart);
            _samplesProcessed += firstLength;
            if (secondPart != nullptr)
            {
                glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, firstLength * sizeof(SampleValueType), secondLength * sizeof(SampleValueType), secondPart);
                _samplesProcessed += secondLength;
            }
            
        }
#else
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, toWrite * sizeof(SampleValueType), _feedbackData);
        RingBuffer.write(_feedbackData, toWrite);
        _samplesProcessed += toWrite;
#endif
        
#if LOGENABLED
        std::cerr << "[ProcessingThread]: processed " << toWrite << "samples" << std::endl;
#endif
        glProgramUniform1i(_DSPProgram, _samplesProcessedUniformLoc, _samplesProcessed);
    }
};

class AnotherSandboxProjectApp : public App
{
protected:
    GPUDSPController* _DSPController;
    ExternalDSPNodeRef externalDSPNode;
    
  public:
    ~AnotherSandboxProjectApp();
    
	void setup() override;
	void mouseDown( MouseEvent event ) override;
    void keyDown( KeyEvent event ) override;
	void update() override;
	void draw() override;
};


AnotherSandboxProjectApp::~AnotherSandboxProjectApp()
{
    delete _DSPController;
}


std::string readAllText(std::string const& path)
{
    std::ifstream ifstr(path);
    if (!ifstr)
        return std::string();
    std::stringstream text;
    ifstr >> text.rdbuf();
    return text.str();
}

const char* getErrorString(cl_int error)
{
    switch(error)
    {
            // run-time and JIT compiler errors
        case 0: return "CL_SUCCESS";
        case -1: return "CL_DEVICE_NOT_FOUND";
        case -2: return "CL_DEVICE_NOT_AVAILABLE";
        case -3: return "CL_COMPILER_NOT_AVAILABLE";
        case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case -5: return "CL_OUT_OF_RESOURCES";
        case -6: return "CL_OUT_OF_HOST_MEMORY";
        case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case -8: return "CL_MEM_COPY_OVERLAP";
        case -9: return "CL_IMAGE_FORMAT_MISMATCH";
        case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case -11: return "CL_BUILD_PROGRAM_FAILURE";
        case -12: return "CL_MAP_FAILURE";
        case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
        case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
        case -15: return "CL_COMPILE_PROGRAM_FAILURE";
        case -16: return "CL_LINKER_NOT_AVAILABLE";
        case -17: return "CL_LINK_PROGRAM_FAILURE";
        case -18: return "CL_DEVICE_PARTITION_FAILED";
        case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
            
            // compile-time errors
        case -30: return "CL_INVALID_VALUE";
        case -31: return "CL_INVALID_DEVICE_TYPE";
        case -32: return "CL_INVALID_PLATFORM";
        case -33: return "CL_INVALID_DEVICE";
        case -34: return "CL_INVALID_CONTEXT";
        case -35: return "CL_INVALID_QUEUE_PROPERTIES";
        case -36: return "CL_INVALID_COMMAND_QUEUE";
        case -37: return "CL_INVALID_HOST_PTR";
        case -38: return "CL_INVALID_MEM_OBJECT";
        case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case -40: return "CL_INVALID_IMAGE_SIZE";
        case -41: return "CL_INVALID_SAMPLER";
        case -42: return "CL_INVALID_BINARY";
        case -43: return "CL_INVALID_BUILD_OPTIONS";
        case -44: return "CL_INVALID_PROGRAM";
        case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case -46: return "CL_INVALID_KERNEL_NAME";
        case -47: return "CL_INVALID_KERNEL_DEFINITION";
        case -48: return "CL_INVALID_KERNEL";
        case -49: return "CL_INVALID_ARG_INDEX";
        case -50: return "CL_INVALID_ARG_VALUE";
        case -51: return "CL_INVALID_ARG_SIZE";
        case -52: return "CL_INVALID_KERNEL_ARGS";
        case -53: return "CL_INVALID_WORK_DIMENSION";
        case -54: return "CL_INVALID_WORK_GROUP_SIZE";
        case -55: return "CL_INVALID_WORK_ITEM_SIZE";
        case -56: return "CL_INVALID_GLOBAL_OFFSET";
        case -57: return "CL_INVALID_EVENT_WAIT_LIST";
        case -58: return "CL_INVALID_EVENT";
        case -59: return "CL_INVALID_OPERATION";
        case -60: return "CL_INVALID_GL_OBJECT";
        case -61: return "CL_INVALID_BUFFER_SIZE";
        case -62: return "CL_INVALID_MIP_LEVEL";
        case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
        case -64: return "CL_INVALID_PROPERTY";
        case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
        case -66: return "CL_INVALID_COMPILER_OPTIONS";
        case -67: return "CL_INVALID_LINKER_OPTIONS";
        case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";
            
            // extension errors
        case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
        case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
        case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
        case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
        case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
        case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
        default: return "Unknown OpenCL error";
    }
}

void logErrorString(cl_int error)
{
    std::cerr << "[OpenCL error]: " << getErrorString(error) << std::endl;
}

void AnotherSandboxProjectApp::setup()
{
    /*
    auto ctx = ci::audio::master();
    ci::audio::OutputNodeRef outputNode = ctx->getOutput();
    
    const size_t sampleRate = outputNode->getSampleRate();
    const size_t bufferSize = outputNode->getFramesPerBlock();
    
    const size_t audioBuffersInGPUBuffer = 4;
    const size_t GPUBuffersInRingBuffer = 2;
    _DSPController = new GPUDSPController(sampleRate, bufferSize * audioBuffersInGPUBuffer * GPUBuffersInRingBuffer);
    
    externalDSPNode = ctx->makeNode(new ExternalDSPNode(&_DSPController->RingBuffer));
    ci::audio::GainNodeRef gainNode = ctx->makeNode(new GainNode(1.0));
    
    externalDSPNode >> gainNode >> outputNode;
    
    externalDSPNode->enable();
    outputNode->enable();
     */
    
    /* получить доступные платформы */
    cl_platform_id platform_id;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    logErrorString(ret);
    
    cl_device_id device_id;
    cl_uint ret_num_devices;
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);
    logErrorString(ret);
    
    /* создать контекст */
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    logErrorString(ret);
    /* создаем команду */
    cl_command_queue command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
    logErrorString(ret);
    
    
    
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    
    std::string src =   "__kernel void test(__global int* message) \
                        {\
                            int gid = get_global_id(0);\
                            __local x[512];\
                            x[gid] = message[gid];\
    \
                            for (int i = 0; i < 4096; ++i)\
                            {\
                                barrier(CLK_LOCAL_MEM_FENCE);\
                                x[gid] += 1;\
                            }\
    \
                            barrier(CLK_LOCAL_MEM_FENCE);\
                            message[gid] = x[gid];\
                        }\0";
    
    std::string srcString = readAllText(PlatformCocoa::get()->getResourcePath("Processing.cl").string());
    const char* str = src.c_str();
    size_t sourceSize = src.length();
    
    /* создать бинарник из кода программы */
    program = clCreateProgramWithSource(context, 1, &str, &sourceSize, &ret);
    logErrorString(ret);
    /* скомпилировать программу */
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    logErrorString(ret);
    
    /* создать кернел */
    kernel = clCreateKernel(program, "test", &ret);
    logErrorString(ret);
    
    
    
    cl_mem memobj = NULL;
    int memLenth = 512;
    cl_int* mem = (cl_int *)malloc(sizeof(cl_int) * memLenth);
    for (int i = 0; i < memLenth; ++i)
    {
        mem[i] = 0;
    }
    mem[5] = 1;
    /* создать буфер */
    memobj = clCreateBuffer(context, CL_MEM_READ_WRITE, memLenth * sizeof(cl_int), NULL, &ret);
    logErrorString(ret);
    /* записать данные в буфер */
    ret = clEnqueueWriteBuffer(command_queue, memobj, CL_TRUE, 0, memLenth * sizeof(cl_int), mem, 0, NULL, NULL);
    logErrorString(ret);
    /* устанавливаем параметр */
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&memobj);
    logErrorString(ret);
    
    
    
    size_t global_work_size[1] = { 512 };
    /* выполнить кернел */
    ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, global_work_size, NULL, 0, NULL, NULL);
    logErrorString(ret);
    /* считать данные из буфера */
    ret = clEnqueueReadBuffer(command_queue, memobj, CL_TRUE, 0, memLenth * sizeof(float), mem, 0, NULL, NULL);
    logErrorString(ret);
    
    for (int i = 0; i < global_work_size[0]; ++i)
    {
        std::cerr << "[" << i << "] = " << mem[i] << std::endl;
    }
}

void AnotherSandboxProjectApp::keyDown( KeyEvent event )
{
}

void AnotherSandboxProjectApp::mouseDown( MouseEvent event )
{
}

void AnotherSandboxProjectApp::update()
{
    //_DSPController->generateSamples();
}

void AnotherSandboxProjectApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( AnotherSandboxProjectApp, RendererGl )
