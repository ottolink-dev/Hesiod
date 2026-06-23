
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


![img](../../images/nodes/Ridgelines_hsd_example.png)

Corresponding Hesiod file: [Ridgelines.hsd](../../examples/Ridgelines.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


