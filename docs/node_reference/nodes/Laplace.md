
Laplace Node
============


Laplace smoothing filter reduces noise and smooth out variations in pixel intensity while preserving the overall structure of an image.



![img](../../images/nodes/Laplace_settings.png)


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
|iterations|Integer|Number of successive filtering iterations.|
|sigma|Float|Filtering strength.|

## Example


!!! note "No example yet"
    No example available for this node.
