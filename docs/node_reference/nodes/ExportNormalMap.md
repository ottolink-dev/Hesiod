
ExportNormalMap Node
====================


ExportNormalMap is an operator for exporting the normal map of an heightmap as a PNG image file.



![img](../../images/nodes/ExportNormalMap_settings.png)


## Category


Export
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|16bit|Bool|Whether the output file is a 16bit PNG file (instead of 8bit).|
|Auto Export on Node Update|Bool|Controls whether the output file is automatically written when the node is updated. Default is false. When set to true, the file is saved automatically on updates. If false, use the 'Force Reload' button on the node to manually trigger the export.|
|Flip-X|Bool|No description|
|Flip-Y|Bool|No description|
|fname|Filename|Export file name.|
|Filename Pattern|String|No description|

## Example


![img](../../images/nodes/ExportNormalMap_hsd_example.png)

Corresponding Hesiod file: [ExportNormalMap.hsd](../../examples/ExportNormalMap.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


