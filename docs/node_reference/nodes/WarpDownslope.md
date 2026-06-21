
WarpDownslope Node
==================


Warp the heightmap with a direction and amount based on the local downslope.



![img](../../images/nodes/WarpDownslope_settings.png)


## Category


WIP
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
|amount|Float|Warp scaling with respect to the gradient.|
|radius|Float|Prefiltering radius for gradient computation.|
|reverse|Bool|Reverse warping direction (upslope if true and downslope otherwise).|

## Example


!!! note "No example yet"
    No example available for this node.  
