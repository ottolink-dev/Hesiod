
FlowSimulation Node
===================


No description available



![img](../../images/nodes/FlowSimulation_settings.png)


## Category


Hydrology
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|depth_map|VirtualArray|No description|
|elevation|VirtualArray|No description|
|water_depth_in|VirtualArray|No description|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|water_depth|VirtualArray|Output water depth map representing flooded areas.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Remove Small Flow Regions|Bool|No description|
|Predefined Depth Map|Enumeration|No description|
|Dry-Out Threshold Ratio|Float|No description|
|Simulation Duration|Float|No description|
|Enable Flux Diffusion|Bool|No description|
|Flux Diffusion Strength|Float|No description|
|Enable Filtering|Bool|No description|
|Filter Radius|Float|No description|
|Minimum Lake Radius|Float|No description|
|Solver Iteration Stride|Integer|Grid sampling stride used by the solver. Higher values process the snow field at a lower spatial resolution, reducing computation time at the cost of fine detail.|
|Initial Water Depth|Float|No description|

## Example


![img](../../images/nodes/FlowSimulation_hsd_example.png)

Corresponding Hesiod file: [FlowSimulation.hsd](../../examples/FlowSimulation.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
