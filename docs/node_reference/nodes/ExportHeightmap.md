
ExportHeightmap Node
====================


ExportHeightmap is an operator for exporting a heightmap in various file formats.



![img](../../images/nodes/ExportHeightmap_settings.png)


## Category


Export
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|VirtualArray|Input heightmap.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|Add Project Name as Prefix|Bool|No description|
|Auto Export on Node Update|Bool|Controls whether the output file is automatically written when the node is updated. Default is false. When set to true, the file is saved automatically on updates. If false, use the 'Force Reload' button on the node to manually trigger the export.|
|Filename|Filename|Export file name.|
|Force Export Shape|Choice|No description|
|File Format|Enumeration|Export format. Available values: png (16 bit), png (8 bit), raw (16 bit, Unity).|

## Example


![img](../../images/nodes/ExportHeightmap_hsd_example.png)

Corresponding Hesiod file: [ExportHeightmap.hsd](../../examples/ExportHeightmap.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).


