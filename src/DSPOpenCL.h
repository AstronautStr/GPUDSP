//
//  DSPOpenCL.h
//  AnotherSandboxProject
//
//  Created by Ilya Solovyov on 08.04.16.
//
//

#ifndef DSPOpenCL_h
#define DSPOpenCL_h

#include "Utils.h"
#include <OpenCL/OpenCL.h>
#include "cinder/app/cocoa/PlatformCocoa.h"

typedef cl_double        DSPSampleType;
typedef cl_double4       DSPSampleType4;

#if UNSAFEBUFFER
#include "UnsafeRingBuffer.h"
typedef UnsafeRingBufferT<DSPSampleType> RingBuffer;
#else
#include "cinder/audio/audio.h"
#include "cinder/audio/dsp/Dsp.h"
typedef ci::audio::dsp::RingBufferT<DSPSampleType> RingBuffer;
#endif

class DSPOpenCL
{
private:    
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
#if LOGENABLED
        std::cerr << "[OpenCL error]: " << getErrorString(error) << std::endl;
#endif
    }
    
    
protected:
    cl_device_id        deviceID;
    cl_context          context;
    cl_command_queue    commandQueue;
    
    cl_program          program;
    cl_kernel           cellsKernel;
    cl_kernel           soundKernel;
    
    DSPSampleType*      samples;
    cl_mem              samplesMemoryObj;
    size_t              samplesMemoryLength;
    
    DSPSampleType*      waveTable;
    cl_mem              waveTableMemoryObj;
    size_t              waveTableMemoryLength;
    
    DSPSampleType4*     cells;
    size_t              cellsCount;
    cl_mem              cellsMemoryObj;
    size_t              cellsMemoryLength;

    cl_mem              rulesMemoryObject;
    size_t              rulesMemoryLength;
    cl_float*           rules;
    
    cl_uint2            gridSize;
    
    cl_uint             samplesProcessed;
    cl_uint             sampleRate;
    cl_uint             samplesToWrite;
    size_t              bufferSize;
    
    void _prepareContext()
    {
        cl_platform_id platformID;
        cl_uint numPlatforms;
        cl_int ret = clGetPlatformIDs(1, &platformID, &numPlatforms);
        logErrorString(ret);
        
        cl_uint numDevices;
        ret = clGetDeviceIDs(platformID, CL_DEVICE_TYPE_GPU, 1, &deviceID, &numDevices);
        logErrorString(ret);
        
#if LOGENABLED
        size_t extInfoSize = 0;
        clGetDeviceInfo(deviceID, CL_DEVICE_EXTENSIONS, NULL, NULL, &extInfoSize);
        logErrorString(ret);
        char* info = new char[extInfoSize];
        clGetDeviceInfo(deviceID, CL_DEVICE_EXTENSIONS, extInfoSize, info, NULL);
        logErrorString(ret);
        std::cout << "[OpenCL SUPPORTED EXTENSIONS] : " << info << std::endl;
        delete [] info;
#endif
        
        context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
        logErrorString(ret);
        
        commandQueue = clCreateCommandQueue(context, deviceID, 0, &ret);
        logErrorString(ret);
    }
    
    void _prepareKernel(cl_kernel* kernelPtr, const std::string& sourceFile)
    {
        cl_int ret = 0;
        program = NULL;
        *kernelPtr = NULL;
        
        std::string clSrcString = readAllText(cinder::app::PlatformCocoa::get()->getResourcePath(sourceFile).string());
        const char* str = clSrcString.c_str();
        size_t sourceSize = clSrcString.length();
        
        program = clCreateProgramWithSource(context, 1, &str, &sourceSize, &ret);
        logErrorString(ret);
        ret = clBuildProgram(program, 1, &deviceID, NULL, NULL, NULL);
        logErrorString(ret);
        
#if LOGENABLED
        size_t len = 0;
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, NULL, NULL, &len);
        char* log = new char[len];
        clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, len, log, NULL);
        std::cerr << log << std::endl;
        delete [] log;
#endif
        
        *kernelPtr = clCreateKernel(program, "kernelMain", &ret);
        logErrorString(ret);
        
        clReleaseProgram(program);
    }
    
    void _setupKernelVars(cl_kernel targetKernel)
    {
        cl_int ret = clSetKernelArg(targetKernel, 0, sizeof(cl_mem), (void*)&samplesMemoryObj);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 1, sizeof(cl_mem), (void*)&waveTableMemoryObj);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 2, sizeof(cl_uint), (void*)&sampleRate);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 3, sizeof(cl_uint), (void*)&samplesProcessed);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 4, sizeof(cl_uint), (void*)&samplesToWrite);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 5, sizeof(cl_mem), (void*)&cellsMemoryObj);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 6, sizeof(cl_mem), (void*)&rulesMemoryObject);
        logErrorString(ret);
        ret = clSetKernelArg(targetKernel, 7, sizeof(cl_uint2), (void*)&gridSize);
        logErrorString(ret);
    }
    
    void _prepareMemory()
    {
        cl_int ret = 0;
        srand(time(0));
        
        // samples
        samplesMemoryObj = NULL;
        samplesMemoryLength = bufferSize;
        samples = new DSPSampleType[samplesMemoryLength];
        for (int i = 0; i < samplesMemoryLength; ++i)
            samples[i] = 0;
        samplesMemoryObj = clCreateBuffer(context, CL_MEM_READ_WRITE, samplesMemoryLength * sizeof(DSPSampleType), NULL, &ret);
        logErrorString(ret);        ret = clEnqueueWriteBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, samplesMemoryLength * sizeof(DSPSampleType), samples, 0, NULL, NULL);
        logErrorString(ret);
        
        // wavetable
        waveTableMemoryObj = NULL;
        waveTableMemoryLength = sampleRate;
        waveTable = new DSPSampleType[waveTableMemoryLength];
        for (int i = 0; i < sampleRate; ++i)
            waveTable[i] = (float)(sin(2.0 * M_PI * (double)i / (double)sampleRate));
        waveTableMemoryObj = clCreateBuffer(context, CL_MEM_READ_WRITE, waveTableMemoryLength * sizeof(DSPSampleType), NULL, &ret);
        logErrorString(ret);
        ret = clEnqueueWriteBuffer(commandQueue, waveTableMemoryObj, CL_TRUE, 0, waveTableMemoryLength * sizeof(DSPSampleType), waveTable, 0, NULL, NULL);
        logErrorString(ret);
        
        rulesMemoryObject = NULL;
        rulesMemoryLength = 5;
        rules = new cl_float[rulesMemoryLength];
        rules[0] = 0.7f;
        rules[1] = 0.5f;
        rules[2] = -0.7f;
        rules[3] = 0.4f;
        rules[4] = 1.0f;
        rulesMemoryObject = clCreateBuffer(context, CL_MEM_READ_WRITE, rulesMemoryLength * sizeof(cl_float), NULL, &ret);
        logErrorString(ret);
        ret = clEnqueueWriteBuffer(commandQueue, rulesMemoryObject, CL_TRUE, 0, rulesMemoryLength * sizeof(cl_float), rules, 0, NULL, NULL);
        logErrorString(ret);
        
        // cells
        gridSize = { 16, 16 };
        cellsCount = gridSize.s[0] * gridSize.s[1];
        cellsMemoryLength = cellsCount * bufferSize;
        cells = new DSPSampleType4[cellsMemoryLength];
        DefferedUpdateGrid = new DSPSampleType4[cellsCount];
        for (int i = 0; i < cellsMemoryLength; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                cells[i].s[j] = 0.0;
                DefferedUpdateGrid[i % cellsCount].s[j] = 0.0;
            }
        }
        
        for (int i = 0; i < cellsCount; ++i)
        {
            cells[i].s[0] = randAmp();
            cells[i].s[1] = randFreq();
        }
        
        cellsMemoryObj = clCreateBuffer(context, CL_MEM_READ_WRITE, cellsMemoryLength * sizeof(DSPSampleType4), NULL, &ret);
        logErrorString(ret);
        ret = clEnqueueWriteBuffer(commandQueue, cellsMemoryObj, CL_TRUE, 0, cellsMemoryLength * sizeof(DSPSampleType4), cells, 0, NULL, NULL);
        logErrorString(ret);
        
        _setupKernelVars(cellsKernel);
        _setupKernelVars(soundKernel);
    }

    
    size_t _getBufferToWrite()
    {
#if FIXEDBUFFER
        return bufferSize;
#else
        return RingBuffer.getAvailableWrite();
#endif
    }

    void _updateSamplesProcessed()
    {
        clSetKernelArg(cellsKernel, 3, sizeof(cl_uint), (void*)&samplesProcessed);
        clSetKernelArg(soundKernel, 3, sizeof(cl_uint), (void*)&samplesProcessed);
    }
    
    void _updateSamplesToWrite(size_t newValue)
    {
        samplesToWrite = (cl_uint)newValue;
        clSetKernelArg(cellsKernel, 4, sizeof(cl_uint), (void*)&samplesToWrite);
        clSetKernelArg(soundKernel, 4, sizeof(cl_uint), (void*)&samplesToWrite);
    }
    
    void _applyDefferedUpdateGrid()
    {
        for (int i = 0; i < cellsCount; ++i)
        {
            float replaceMask = DefferedUpdateGrid[i].s[0] > 0.0f ? 1.0f : 0.0f;
            float clearMask = DefferedUpdateGrid[i].s[0] < 0.0f ? 1.0f : 0.0f;
            
            for (int j = 0; j < 4; ++j)
            {
                cells[i].s[j] = (replaceMask * DefferedUpdateGrid[i].s[j] + (1.0f - replaceMask) * cells[i].s[j]) * (1.0f - clearMask);
                DefferedUpdateGrid[i].s[j] = 0.0f;
            }
        }
        
        clEnqueueWriteBuffer(commandQueue, cellsMemoryObj, CL_TRUE, 0, cellsCount * sizeof(DSPSampleType4), cells, 0, NULL, NULL);
    }
    
public:
    RingBuffer RingBuffer;
    DSPSampleType4*   DefferedUpdateGrid;
    
    DSPOpenCL(size_t initSampleRate, size_t initBufferSize) :
    RingBuffer(initBufferSize)
    {
        this->samplesProcessed = 0;
        this->sampleRate = (cl_uint)initSampleRate;
        this->bufferSize = initBufferSize;
        this->samplesToWrite = (cl_uint)initBufferSize;
        
        _prepareContext();
        _prepareKernel(&cellsKernel, "Cells.ncl");
        _prepareKernel(&soundKernel, "Processing.ncl");
        _prepareMemory();
    }
    
    float* getRulesBirthCenter()
    {
        return &rules[0];
    }
    float* rulesBirthRadius()
    {
        return &rules[1];
    }
    float* rulesKeepCenter()
    {
        return &rules[2];
    }
    float* rulesKeepRadius()
    {
        return &rules[3];
    }
    float* rulesSpeed()
    {
        return &rules[4];
    }
    
    ~DSPOpenCL()
    {
        delete [] waveTable;
        delete [] samples;
        delete [] cells;
        delete [] DefferedUpdateGrid;
        
        clReleaseDevice(deviceID);
        clReleaseContext(context);
        clReleaseCommandQueue(commandQueue);
        clReleaseMemObject(cellsMemoryObj);
        clReleaseMemObject(waveTableMemoryObj);
        clReleaseMemObject(samplesMemoryObj);
        
        clReleaseKernel(cellsKernel);
        clReleaseKernel(soundKernel);
    }
    
    DSPSampleType4* getCurrentGridState()
    {
        return &cells[cellsCount * (bufferSize - 1)];
    }
    
    glm::ivec2 getGridSize()
    {
        return glm::ivec2(gridSize.s[0], gridSize.s[1]);
    }
    
    size_t getCellsCount()
    {
        return cellsCount;
    }
    
    void generateSamples(float* data = NULL)
    {
        size_t toWrite = _getBufferToWrite();
        
        if (toWrite <= 0)
            return;
        
        clEnqueueWriteBuffer(commandQueue, rulesMemoryObject, CL_TRUE, 0, rulesMemoryLength * sizeof(cl_float), rules, 0, NULL, NULL);
        _applyDefferedUpdateGrid();
        
        _updateSamplesProcessed();
        _updateSamplesToWrite(toWrite);

        size_t globalWorkSize[1] = { cellsCount };
        clEnqueueNDRangeKernel(commandQueue, cellsKernel, 1, NULL, globalWorkSize, NULL, 0, NULL, NULL);
        
        clEnqueueReadBuffer(commandQueue, cellsMemoryObj, CL_TRUE, 0, cellsMemoryLength * sizeof(DSPSampleType4), cells, 0, NULL, NULL);
        
        globalWorkSize[0] = toWrite;
        clEnqueueNDRangeKernel(commandQueue, soundKernel, 1, NULL, globalWorkSize, NULL, 0, NULL, NULL);
        
#if LOGENABLED
        bool logHard = false;
        if (logHard)
        {
            clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, toWrite * sizeof(DSPSampleType), samples, 0, NULL, NULL);
            
            int tail = toWrite;
            int radius = 10;
            
            for(int i = 0; i < radius; ++i)
            {
                std::cerr << i << " " << samples[i] << std::endl;
            }
            for(int i = tail - radius; i < tail; ++i)
            {
                std::cerr << i << " " << samples[i] << std::endl;
            }
        }
#endif
        
        if (data != NULL)
        {
            clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, toWrite * sizeof(DSPSampleType), data, 0, NULL, NULL);
            samplesProcessed += toWrite;
        }
        else
        {
#if UNSAFEBUFFER
        DSPSampleType* firstPart = nullptr;
        DSPSampleType* secondPart = nullptr;
        size_t firstLength = 0;
        size_t secondLength = 0;
        
        RingBuffer.getUnsafeDataWritePointer(toWrite, firstPart, secondPart, firstLength, secondLength);
        if (firstPart != nullptr)
        {
            clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, firstLength * sizeof(DSPSampleType), firstPart, 0, NULL, NULL);
            samplesProcessed += firstLength;
            if (secondPart != nullptr)
            {
                clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, firstLength * sizeof(DSPSampleType), secondLength * sizeof(DSPSampleType), secondPart, 0, NULL, NULL);
                samplesProcessed += secondLength;
            }
            
        }
#else
        clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, toWrite * sizeof(DSPSampleType), samples, 0, NULL, NULL);

        RingBuffer.write(samples, toWrite);
        samplesProcessed += toWrite;
#endif
        }
#if LOGENABLED
        std::cerr << "[ProcessingThread]: processed " << toWrite << "samples" << std::endl;
#endif
    }
};

#endif /* DSPOpenCL_h */
