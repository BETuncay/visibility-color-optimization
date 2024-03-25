# visibility-color-optimization

A visualization program for dense line sets.

It is based on the paper "Decoupled Opacity Optimization for Points, Lines and Surfaces" by Tobias Günther, Holger Theisel and Markus Gross from the Computer Graphics Forum (Proc. Eurographics), 2017.

An agglomerative hierarchical clustering step was added to improve the line selection.
The core opacity optimization functionality was forked from a demo implementation.
Lastly, an optimal color mapping algorithm based on Histogram Equalization is utilized to achive even usage of a colormap.

This repository only contains the implementation for the visualization program and another program for the extraction of streamlines.
Velocity field datasets must be provides seperately in the Amira format.
 
## Requirements
- Windows SDK
- DirectX 11
- CMake 3.8

## Installation
The repository contains two seperate projects.
The first is dedicated to the visualization of streamlines via clustering, opacity optimization and color optimization.
The second is dedicated to the extraction of streamlines from flow fields, scalar properties of the lines and the distance matrix for clustering.

The following guide makes use of the CMAKE gui application.

### Line Visualization Program

1. Clone the repository to the desired location via ```git clone https://gitlab.cs.fau.de/i9vc/students/ws23/visibility-color-optimization.git```
2. Open the CMAKE gui application
3. Select the source directory "visibility-color-optimization" in CMAKE
4. Create a build folder in the source direction
5. Select the build folder as the build location in CMAKE
6. Press the Configure Button (some red warnings will appear, ignore them)
7. Select your Visual Studio Version (or alternatives) 
8. Press the Generate Button
9. Open the Project
10. Build the Configuration: "ALL_BUILD"
11. Select "vc_optimization" as startup projects
12. Create two folders in source directory named: "data", "distanceMatrix"
13. Insert your obj files into the "data" folder 
14. Insert your dist files into the "distanceMatrix" folder 

### Line Extraction Program
1. Clone the repository to the desired location via ```git clone https://gitlab.cs.fau.de/i9vc/students/ws23/visibility-color-optimization.git```
2. Create the folder "raw_data" inside the "Transformer" directory and insert the Amira files
3. Edit the CMakeList.txt so that the "AM_SOURCES" match the amira file paths.
4. Open the CMAKE gui application
5. Select the source directory "Transformer" in CMAKE (raw_data/Transformer)
6. Create a build folder in the "Transformer" direction
7. Select the build folder as the build location in CMAKE
8. Press the Configure Button
9. Select your Visual Studio Version (or alternatives) 
10. Press the Generate Button (some red warnings will appear, ignore them)
11. Open the Project
12. Build the Configuration: "ALL_BUILD"
13. Select "transformer" as startup projects

## Usage

### Adding new files
In the installation section, it is mentioned that there some special folders needed to be created to place obj / dist files.
These folders are included in the CMakeList via GLOB.
Therefore, when a new file is added to those folder, then it is necessary to change the CMakeList.txt and then rebuild.
Otherwise, the CMakeList can not detect the new file and it is not included in the build.

### Line Visualization Program
It is necessary to place at least one obj file in the "data" folder.
To enable clustering, a matching dist file must be in the "distanceMatrix" folder.

There are two files to interface with for the visualization program.

main.cpp
- ENABLE_IMGUI Macro: enable/disable the gui
- path/distanceMatrixPath: path to the selected obj / dist file
- eye / lookAt: initial camera setup
- resolution
- q, r, lambda, smoothingIterations: opacity optimization energy equation parameters
- totalNumCPs: num points for the line segmentation
- stripWidth: line width
- clusterCount: top k clusters to select
- representativeMethod: cluster representative representative method
- histogramMode: color optimization strategy
- segments: segment boundary points (used with segmented histogram equalization)

shader_Common_hlsli
- HISTOGRAM_BINS: amount of bins to use in the histogram
- HISTOGRAM_CDF_SERIAL / HISTOGRAM_CDF_PARALLEL: method to calculate HE CDF
- HISTOGRAM_SEGMENTS_MAX: maximum amount of segments

In summary, specify the obj dataset you want to visualize by setting the path variable.
Adjust other variables according to need.

Alternatively just utilize the gui

### Line Extraction Program
This program is used to extract streamlines from Amira files along with two scalar values.
Additionally, the distance matrix used for clustering is also extracted here.

It is necessary to place at least one amira file in the "raw_data" folder.

In the main file specify the path to the folders containing the: amira, obj, dist files.
Then, the file is divided into the following sections.
#### Streamline + Scalar values
- Specifiy the configuration for the dataset ("ExtractConfig")
- Start the extraction by enabeling the macro if section

#### Distance Matrix
- Specifiy the configuration for the dataset ("DistanceConfig")
- Start the extraction by enabeling the macro if section

#### Print Line Data
- Specifiy the configuration for the dataset (reuses a subset of the "DistanceConfig")
- Start process by enabeling the macro if section

#### Save Importance / Coloration to file
- Specifiy the configuration for the dataset (reuses a subset of the "DistanceConfig")
- Start process by enabeling the macro if section
