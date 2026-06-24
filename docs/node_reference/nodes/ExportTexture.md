
ExportTexture Node
==================


ExportTexture is a texture to a PNG image file.



![img](../../images/nodes/ExportTexture_settings.png)


## Category


Export
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|texture|VirtualTexture|Input texture.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|16 bit|Bool|Decides whether the output is an 8 bit or a 16 bit PNG file.|
|Add Project Name as Prefix|Bool|No description|
|Auto Export on Node Update|Bool|Controls whether the output file is automatically written when the node is updated. Default is false. When set to true, the file is saved automatically on updates. If false, use the 'Force Reload' button on the node to manually trigger the export.|
|fname|Filename|Export file name.|

## Example


![img](../../images/nodes/ExportTexture_hsd_example.png)

Corresponding Hesiod file: [ExportTexture.hsd](../../examples/ExportTexture.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
