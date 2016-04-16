#ifdef cl_khr_fp64
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
typedef     double      DSPSampleType;
typedef     double2     DSPSampleType2;
typedef     double4     DSPSampleType4;
#else
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
typedef     float       DSPSampleType;
typedef     float2      DSPSampleType2;
typedef     float4      DSPSampleType4;
#endif


DSPSampleType waveTableOsc(__global DSPSampleType* waveTable, DSPSampleType frequency, uint sampleRate, uint samplesProcessed, uint samplePosition);

uint ringIndex(int index, uint size);
uint2 torIndex(int2 index, uint2 size);

DSPSampleType rand(DSPSampleType2 co);
DSPSampleType randFreq(DSPSampleType fractTime);

DSPSampleType waveTableOsc(__global DSPSampleType* waveTable, DSPSampleType frequency, uint sampleRate, uint samplesProcessed, uint samplePosition)
{
    uint     frecInt     = uint(frequency);
    
    DSPSampleType    frecFract;
    fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(DSPSampleType(samplesProcessed) * frecFract) % sampleRate + uint(DSPSampleType(samplePosition) * frecFract) % sampleRate;
    
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


DSPSampleType rand(DSPSampleType2 co)
{
    DSPSampleType value = 0.0f;
    return fract(sin(dot(co.xy, (DSPSampleType2)(12.9898f, 78.233f))) * 43758.5453f, &value);
}

DSPSampleType randFreq(DSPSampleType fractTime)
{
    DSPSampleType min = 20.0f;
    DSPSampleType max = 22000.0f;
    return pow(2.0f, (log2(min) + (log2(max) - log2(min)) * rand((DSPSampleType2)(fractTime, 1.3217f))));
}

__kernel void kernelMain(__global DSPSampleType* samples, __global DSPSampleType* waveTable, uint sampleRate, uint samplesProcessed, uint bufferSize, __global DSPSampleType4* cells, __global DSPSampleType* rules, uint2 gridSize)
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
        
        DSPSampleType4 cell = cells[cellIndex];
        DSPSampleType signal = cell.x * waveTableOsc(waveTable, cell.y, sampleRate, samplesProcessed, sampleIdx);
        cells[cellIndex].z = signal;
        
        barrier(CLK_GLOBAL_MEM_FENCE);
        
        globalID = get_global_id(0);
        cellIndex = sampleIdx * gridSize.x * gridSize.y + globalID;

        cell = cells[cellIndex];
        cellPosition = (uint2)(globalID / gridSize.y, globalID % gridSize.y);
        
        DSPSampleType sum = 0.0f;
        int ruleRadius = 1;
        for (int i = -ruleRadius; i <= ruleRadius; ++i)
        {
            for (int j = - ruleRadius; j <= ruleRadius; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                
                uint2 broIdx = torIndex((int2)cellPosition + (int2)(i, j), gridSize);
                DSPSampleType4 bro = cells[(broIdx.x * gridSize.y + broIdx.y) + sampleIdx * gridSize.x * gridSize.y];
                
                sum += bro.x * bro.z;
            }
        }
        
        DSPSampleType deltaSign = -1.0f;
        DSPSampleType deltaValue = rules[4] / (DSPSampleType)sampleRate;
        DSPSampleType rulesBirthCenter = rules[0];
        DSPSampleType rulesBirthRadius = rules[1];
        DSPSampleType rulesKeepCenter = rules[2];
        DSPSampleType rulesKeepRadius = rules[3];
        
        if (sum >= rulesBirthCenter - rulesBirthRadius && sum <= rulesBirthCenter + rulesBirthRadius)
            deltaSign = 1.0f;
        else if (sum >= rulesKeepCenter - rulesKeepRadius && sum <= rulesKeepCenter + rulesKeepRadius)
            deltaSign = 0.0f;
        
        int cellNextStepIndex = globalID + ((sampleIdx + 1) % bufferSize) * gridSize.x * gridSize.y;

        DSPSampleType newAmp = clamp(cell.x + deltaValue * deltaSign, 0.0f, 1.0f);
        DSPSampleType randFreqMask = floor(1.0f - newAmp);
        cells[cellNextStepIndex].x = newAmp;
        cells[cellNextStepIndex].y = cell.y * (1 - randFreqMask) + randFreq((DSPSampleType)((samplesProcessed % sampleRate + sampleIdx) % sampleRate) / (DSPSampleType)sampleRate) * randFreqMask;
        
        barrier(CLK_GLOBAL_MEM_FENCE);
    }
}