#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#define FLOATING_POINT_TYPE double

typedef SampleType      FLOATING_POINT_TYPE;
typedef SampleType2     FLOATING_POINT_TYPE2;
typedef SampleType4     FLOATING_POINT_TYPE4;

SampleType waveTableOsc(__global SampleType* waveTable, SampleType frequency, uint sampleRate, uint samplesProcessed, uint samplePosition);

uint ringIndex(int index, uint size);
uint2 torIndex(int2 index, uint2 size);

SampleType rand(SampleType2 co);
SampleType randFreq(SampleType fractTime);

SampleType waveTableOsc(__global SampleType* waveTable, SampleType frequency, uint sampleRate, uint samplesProcessed, uint samplePosition)
{
    uint     frecInt     = uint(frequency);
    SampleType    frecFract;  fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(SampleType(samplesProcessed) * frecFract) % sampleRate + uint(SampleType(samplePosition) * frecFract) % sampleRate;
    
    uint     tableIndex  = (intPart + fractPart) % sampleRate;
    
    return waveTable[tableIndex];
}

uint ringIndex(int index, uint size)
{
    return (size + index) % size;
}

uint2 torIndex(int2 index, uint2 size)
{
    return (uint2)(ringIndex(index.x, size.x), ringIndex(index.y, size.y));
}


SampleType rand(SampleType2 co)
{
    SampleType value = 0.0f;
    return fract(sin(dot(co.xy, (SampleType2)(12.9898f, 78.233f))) * 43758.5453f, &value);
}

SampleType randFreq(SampleType fractTime)
{
    SampleType min = 20.0f;
    SampleType max = 22000.0f;
    return pow(2.0f, (log2(min) + (log2(max) - log2(min)) * rand((SampleType2)(fractTime, 1.3217f))));
}

__kernel void kernelMain(__global SampleType* samples, __global SampleType* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global SampleType4* cells, __global SampleType* rules, uint2 gridSize)
{
    // cells is gridSize.x * gridSize.y * bufferSize length
    uint globalID = get_global_id(0);
    
    uint2 cellPosition = (uint2)(globalID / gridSize.y, globalID % gridSize.y);
    if (cellPosition.x >= gridSize.x || cellPosition.y >= gridSize.y)
        return;
    
    for (uint sampleIdx = 0; sampleIdx < bufferSize; ++sampleIdx)
    {
        globalID = get_global_id(0);
        uint cellIndex = sampleIdx * gridSize.x * gridSize.y + globalID;
        
        SampleType4 cell = cells[cellIndex];
        SampleType signal = cell.x * waveTableOsc(waveTable, cell.y, sampleRate, samplesProcessed, sampleIdx);
        cells[cellIndex].z = signal;
        
        barrier(CLK_GLOBAL_MEM_FENCE);
        
        globalID = get_global_id(0);
        cellIndex = sampleIdx * gridSize.x * gridSize.y + globalID;

        cell = cells[cellIndex];
        cellPosition = (uint2)(globalID / gridSize.y, globalID % gridSize.y);
        
        SampleType sum = 0.0f;
        int ruleRadius = 1;
        for (int i = -ruleRadius; i <= ruleRadius; ++i)
        {
            for (int j = - ruleRadius; j <= ruleRadius; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                
                uint2 broIdx = torIndex((int2)cellPosition + (int2)(i, j), gridSize);
                SampleType4 bro = cells[(broIdx.x * gridSize.y + broIdx.y) + sampleIdx * gridSize.x * gridSize.y];
                
                sum += bro.x * bro.z;
            }
        }
        
        SampleType deltaSign = -1.0f;
        SampleType deltaValue = rules[4] / (SampleType)sampleRate;
        SampleType rulesBirthCenter = rules[0];
        SampleType rulesBirthRadius = rules[1];
        SampleType rulesKeepCenter = rules[2];
        SampleType rulesKeepRadius = rules[3];
        
        if (sum >= rulesBirthCenter - rulesBirthRadius && sum <= rulesBirthCenter + rulesBirthRadius)
            deltaSign = 1.0f;
        else if (sum >= rulesKeepCenter - rulesKeepRadius && sum <= rulesKeepCenter + rulesKeepRadius)
            deltaSign = 0.0f;
        
        int cellNextStepIndex = globalID + ((sampleIdx + 1) % bufferSize) * gridSize.x * gridSize.y;

        SampleType newAmp = clamp(cell.x + deltaValue * deltaSign, 0.0f, 1.0f);
        SampleType randFreqMask = floor(1.0f - newAmp);
        cells[cellNextStepIndex].x = newAmp;
        cells[cellNextStepIndex].y = cell.y * (1 - randFreqMask) + randFreq((SampleType)((samplesProcessed % sampleRate + sampleIdx) % sampleRate) / (SampleType)sampleRate) * randFreqMask;
        
        barrier(CLK_GLOBAL_MEM_FENCE);
    }
}