
MakePeriodic Node
=================


Makes the input heightmap tileable by creating a smooth linear transition at the boundaries. This is useful for generating seamless textures or terrains that can be repeated without visible seams.



![img](../../images/nodes/MakePeriodic_settings.png)


## Category


Operator/Tiling
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|The input heightmap to be made tileable.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|The output heightmap with smooth transitions at the boundaries, making it tileable.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|overlap|Float|The ratio of overlap at the boundaries. A higher value creates a smoother transition but may reduce the usable area of the heightmap.|
|periodicity_type|Enumeration|Describes the periodicity mode applied along map axes. The domain repeats along the X axis, the Y axis, or both.|

## Example


![img](../../images/nodes/MakePeriodic_hsd_example.png)

Corresponding Hesiod file: [MakePeriodic.hsd](../../examples/MakePeriodic.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


