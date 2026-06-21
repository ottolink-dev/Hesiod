
RecastCliff Node
================


RecastCliff add cliffs in a heightmap by introducing sharp changes in elevation to simulate steep vertical terrain features such as cliffs or escarpments.



![img](../../images/nodes/RecastCliff_settings.png)


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
|amplitude|Float|Cliff amplitude.|
|gain|Float|Gain, influence the cliff elevation profile.|
|radius|Float|Filter influence radius.|
|talus_global|Float|Reference talus at which the cliff are added.|

## Example


!!! note "No example yet"
    No example available for this node.  
