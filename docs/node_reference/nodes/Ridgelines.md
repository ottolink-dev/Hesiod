
Ridgelines Node
===============


Ridgelines generates an heightmap assuming the input path defines a ridgeline.



![img](../../images/nodes/Ridgelines_settings.png)


## Category


Primitive/Authoring
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|dx|VirtualArray|Displacement with respect to the domain size (x-direction).|
|dy|VirtualArray|Displacement with respect to the domain size (y-direction).|
|path|Path|Set of points (x, y) and elevations z.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|heightmap|VirtualArray|Interpolated heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|k_smoothing|Float|Smoothing intensity.|
|talus_global|Float|Ridge slope.|
|vmin|Float|Minimum value (lower values are clamped).|
|width|Float|Ridge edge width.|

## Example


!!! note "No example yet"
    No example available for this node.
