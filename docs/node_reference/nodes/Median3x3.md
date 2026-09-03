
Median3x3 Node
==============


Median3x3 filter is a median filter with a 3x3 kernel used to reduce noise while preserving edges in an image. This process removes outliers and smooths the image without noise reduction and feature preservation in digital images.



![img](../../images/nodes/Median3x3_settings.png)


## Category


Filter/Smoothing
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap.|
|mask|VirtualArray|Mask defining the filtering intensity (expected in [0, 1]).|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|VirtualArray|Filtered heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|GPU|Bool|Toogle GPU acceleration on or off.|

## Example


![img](../../images/nodes/Median3x3_hsd_example.png)

Corresponding Hesiod file: [Median3x3.hsd](../../examples/Median3x3.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


