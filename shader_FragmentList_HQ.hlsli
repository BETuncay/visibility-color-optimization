struct FragmentData
{
    uint nDepth; // Depth 
    uint nCoverage; // Coverage
    float ScalarColor;
    float Transparency;
    float Transmittance;
    uint fDifffSpec;
    float halfDistCenter;
};

struct FragmentLink
{
    FragmentData fragmentData; // Fragment data
    uint nNext; // Link to next fragment
};