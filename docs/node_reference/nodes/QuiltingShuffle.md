
QuiltingShuffle Node
====================


QuiltingShuffle performs a quilting procedure and generates a new image by seamlessly stitching together patches from an existing image.



![img](../../images/nodes/QuiltingShuffle_settings.png)


## Category


Operator/Resynthesis
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Synthetized heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|filter_width_ratio|Float|Smooth filter width with respect the overlap length.|
|overlap|Float|Overlap ratio between patches.|
|patch_flip|Bool|Allow patch flipping.|
|patch_rotate|Bool|Allow patch 90 degree rotation.|
|patch_transpose|Bool|Allow patch tranposition.|
|patch_width|Float|Decide on the size (with respect to the domain size) of the patches that will be used to create the new heightmap. This determines the level of detail and texture in the final result.|
|Seed|Random seed number|Random seed number. The random seed is an offset to the randomized process. A different seed will produce a new result.|

## Example


![img](../../images/nodes/QuiltingShuffle_hsd_example.png)

Corresponding Hesiod file: [QuiltingShuffle.hsd](../../examples/QuiltingShuffle.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


