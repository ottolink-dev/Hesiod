
WaterElevationFromDepth Node
============================


Computes the absolute water elevation by adding the terrain elevation and the water depth. This simplifies workflows where only depth is available. The resulting water elevation can be used, for example, when exporting water as an asset with ExportAsset.



![img](../../images/nodes/WaterElevationFromDepth_settings.png)


## Category


Hydrology
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|elevation|VirtualArray|Input terrain elevation values.|
|water_depth|VirtualArray|Output water depth map representing flooded areas.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|water_elevation|VirtualArray|Output absolute water elevation computed as elevation + water_depth.|

## Example


!!! note "No example yet"
    No example available for this node.
