__kernel void osc(__global float* samples, __global float* waveTable, uint sampleRate, uint samplesProcessed)
{
    uint samplePosition = get_global_id(0);

    float    frequency   = 74.93f;
    uint     frecInt     = uint(frequency);
    float    frecFract;  fract(frequency, &frecFract);
    
    uint     SPmSR       = samplesProcessed % sampleRate;
    uint     FImSR       = frecInt % sampleRate;
    uint     SImSR       = samplePosition % sampleRate;
    uint     intPart     = (SPmSR * FImSR) % sampleRate + (SImSR * FImSR) % sampleRate;
    uint     fractPart   = uint(float(samplesProcessed) * frecFract) % sampleRate + uint(float(samplePosition) * frecFract) % sampleRate;
    
    uint     tableIndex  = (intPart + fractPart) % sampleRate;
    
    samples[samplePosition] = waveTable[tableIndex];
}