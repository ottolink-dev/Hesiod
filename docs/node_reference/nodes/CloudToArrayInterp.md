
CloudToArrayInterp Node
=======================


CloudToArrayInterp generates a smooth and continuous 2D elevation map from a set of scattered points using Delaunay linear interpolation.



![img](../../images/nodes/CloudToArrayInterp_settings.png)


## Category


Geometry/Cloud
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|cloud|Cloud|Set of points (x, y) and elevations z.|
|dx|VirtualArray|Displacement with respect to the domain size (x-direction).|
|dy|VirtualArray|Displacement with respect to the domain size (y-direction).|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|heightmap|VirtualArray|Interpolated heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Interpolation Method|Enumeration|No description|

## Example


![img](../../images/nodes/CloudToArrayInterp_hsd_example.png)

Corresponding Hesiod file: [CloudToArrayInterp.hsd](../../examples/CloudToArrayInterp.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
