
ThermalFlatten Node
===================


TODO



![img](../../images/nodes/ThermalFlatten_settings.png)


## Category


Erosion/Deposition
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|TODO|
|mask|VirtualArray|No description|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|TODO|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Duration|Float|No description|
|scale_talus_with_elevation|Bool|Scales the talus amplitude based on heightmap elevation, reducing it at lower elevations and maintaining the nominal value at higher elevations.|
|sigma_inf|Float|No description|
|sigma_sup|Float|No description|
|talus_global|Float|TODO|

## Example


![img](../../images/nodes/ThermalFlatten_hsd_example.png)

Corresponding Hesiod file: [ThermalFlatten.hsd](../../examples/ThermalFlatten.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


