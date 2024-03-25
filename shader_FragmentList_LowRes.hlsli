struct FragmentData
{
    unsigned int uDepth;        // Depth as uint // Depth (24 bit), importance (8 bit) in MinGather_LowRes Shader
    float fAlphaWeight;         // Alpha weight
    float fImportance;          // Importance
};

struct FragmentLink
{
    FragmentData fragmentData;  // Fragment data
    uint nNext;                 // Link to next fragment
};