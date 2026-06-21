
CoastalErosionDiffusion Node
============================


No description available



![img](../../images/nodes/CoastalErosionDiffusion_settings.png)


## Category


Erosion/Water
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|elevation_in|VirtualArray|No description|
|mask|VirtualArray|No description|
|water_depth_in|VirtualArray|No description|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|elevation|VirtualArray|No description|
|water_depth|VirtualArray|Output water depth map representing flooded areas.|
|water_mask|VirtualArray|No description|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Additional Depth|Float|No description|
|Iterations|Integer|No description|

## Example


![img](../../images/nodes/CoastalErosionDiffusion_hsd_example.png)

Corresponding Hesiod file: [CoastalErosionDiffusion.hsd](../../examples/CoastalErosionDiffusion.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
