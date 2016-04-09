//
//  DSPOpenCL.h
//  AnotherSandboxProject
//
//  Created by Ilya Solovyov on 08.04.16.
//
//

#ifndef DSPOpenCL_h
#define DSPOpenCL_h

#include <OpenCL/OpenCL.h>
#include "cinder/app/cocoa/PlatformCocoa.h"

typedef cl_float SampleValueType;

#if UNSAFEBUFFER
#include "UnsafeRingBuffer.h"
typedef UnsafeRingBufferT<SampleValueType> RingBuffer;
#else
#include "cinder/audio/audio.h"
#include "cinder/audio/dsp/Dsp.h"
typedef ci::audio::dsp::RingBufferT<SampleValueType> RingBuffer;
#endif

class DSPOpenCL
{
protected:
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
#if LOGENABLED
        std::cerr << "[OpenCL error]: " << getErrorString(error) << std::endl;
#endif
    }
    
    cl_device_id        deviceID;
    cl_context          context;
    cl_command_queue    commandQueue;
    
    cl_program          program;
    cl_kernel           kernel;
    
    SampleValueType*    waveTable;
    SampleValueType*    samples;
    
    cl_mem              samplesMemoryObj;
    size_t              samplesMemoryLength;
    
    cl_mem              waveTableMemoryObj;
    size_t              waveTableMemoryLength;
    
    cl_uint             samplesProcessed;
    cl_uint             sampleRate;
    size_t              bufferSize;
    
    
    size_t _getBufferSize()
    {
        return bufferSize;
    }
    
    size_t _getBufferToWrite()
    {
        return bufferSize;
        //return RingBuffer.getAvailableWrite();
    }
    
    void _prepareMemory()
    {
        cl_int ret = 0;
        samplesMemoryObj = NULL;
        samplesMemoryLength = _getBufferSize();
        
        samples = new SampleValueType[samplesMemoryLength];
        for (int i = 0; i < samplesMemoryLength; ++i)
            samples[i] = 0;
        
        samplesMemoryObj = clCreateBuffer(context, CL_MEM_READ_WRITE, samplesMemoryLength * sizeof(SampleValueType), NULL, &ret);
        logErrorString(ret);
        ret = clEnqueueWriteBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, samplesMemoryLength * sizeof(SampleValueType), samples, 0, NULL, NULL);
        logErrorString(ret);
        
        
        waveTableMemoryObj = NULL;
        waveTableMemoryLength = sampleRate;
        
        waveTable = new SampleValueType[waveTableMemoryLength];
        for (int i = 0; i < sampleRate; ++i)
            waveTable[i] = (float)(sin(2.0 * M_PI * (double)i / (double)sampleRate));


        waveTableMemoryObj = clCreateBuffer(context, CL_MEM_READ_WRITE, waveTableMemoryLength * sizeof(SampleValueType), NULL, &ret);
        logErrorString(ret);
        ret = clEnqueueWriteBuffer(commandQueue, waveTableMemoryObj, CL_TRUE, 0, waveTableMemoryLength * sizeof(SampleValueType), waveTable, 0, NULL, NULL);
        logErrorString(ret);
        
        ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&samplesMemoryObj);
        logErrorString(ret);
        ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&waveTableMemoryObj);
        logErrorString(ret);
        ret = clSetKernelArg(kernel, 2, sizeof(cl_uint), (void*)&sampleRate);
        logErrorString(ret);
        ret = clSetKernelArg(kernel, 3, sizeof(cl_uint), (void*)&samplesProcessed);
        logErrorString(ret);
    }
    
    void _prepareKernel()
    {
        program = NULL;
        kernel = NULL;
        cl_int ret = 0;
        
        std::string clSrcString = readAllText(cinder::app::PlatformCocoa::get()->getResourcePath("Processing.ncl").string());
        
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
        kernel = clCreateKernel(program, "osc", &ret);
        logErrorString(ret);
    }
    
    void _prepareContext()
    {
        cl_platform_id platformID;
        cl_uint numPlatforms;
        cl_int ret = clGetPlatformIDs(1, &platformID, &numPlatforms);
        logErrorString(ret);
        
        cl_uint numDevices;
        ret = clGetDeviceIDs(platformID, CL_DEVICE_TYPE_DEFAULT, 1, &deviceID, &numDevices);
        logErrorString(ret);
        
        context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &ret);
        logErrorString(ret);

        commandQueue = clCreateCommandQueue(context, deviceID, 0, &ret);
        logErrorString(ret);
    }
    
public:
    RingBuffer RingBuffer;
    
    DSPOpenCL(size_t sampleRate, size_t bufferSize) :
    RingBuffer(bufferSize)
    {
        this->samplesProcessed = 0;
        this->sampleRate = (cl_uint)sampleRate;
        this->bufferSize = bufferSize;
        
        _prepareContext();
        _prepareKernel();
        _prepareMemory();
    }
    
    ~DSPOpenCL()
    {
        delete [] waveTable;
        delete [] samples;
    }
    
    void generateSamples(float* data = NULL)
    {
        cl_int ret = 0;
        size_t toWrite = _getBufferToWrite();
        
        if (toWrite <= 0)
            return;
        
        
        ret = clSetKernelArg(kernel, 3, sizeof(cl_uint), (void*)&samplesProcessed);
        logErrorString(ret);
        
        size_t globalWorkSize[1] = { toWrite };
        ret = clEnqueueNDRangeKernel(commandQueue, kernel, 1, NULL, globalWorkSize, NULL, 0, NULL, NULL);
        logErrorString(ret);
        
        if (data != NULL)
        {
            ret = clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, toWrite * sizeof(SampleValueType), data, 0, NULL, NULL);
            logErrorString(ret);
            samplesProcessed += toWrite;
        }
        else
        {
#if UNSAFEBUFFER
        SampleValueType* firstPart = nullptr;
        SampleValueType* secondPart = nullptr;
        size_t firstLength = 0;
        size_t secondLength = 0;
        
        RingBuffer.getUnsafeDataWritePointer(toWrite, firstPart, secondPart, firstLength, secondLength);
        if (firstPart != nullptr)
        {
            ret = clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, firstLength * sizeof(SampleValueType), firstPart, 0, NULL, NULL);
            logErrorString(ret);
            
            samplesProcessed += firstLength;
            if (secondPart != nullptr)
            {
                ret = clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, firstLength * sizeof(SampleValueType), secondLength * sizeof(SampleValueType), secondPart, 0, NULL, NULL);
                logErrorString(ret);
                
                samplesProcessed += secondLength;
            }
            
        }
#else
        ret = clEnqueueReadBuffer(commandQueue, samplesMemoryObj, CL_TRUE, 0, toWrite * sizeof(SampleValueType), samples, 0, NULL, NULL);
        logErrorString(ret);

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