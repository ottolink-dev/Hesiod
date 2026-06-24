
CloudRemapValues Node
=====================


CloudRemapValues remap the range of the cloud point values.



![img](../../images/nodes/CloudRemapValues_settings.png)


## Category


Geometry/Cloud
## Inputs

|Name|Type|Description|
| :--- | :--- | :--- |
|input|Cloud|Input cloud.|

## Outputs

|Name|Type|Description|
| :--- | :--- | :--- |
|output|Cloud|Output cloud with new value range.|

## Parameters

|Name|Type|Description|
| :--- | :--- | :--- |
|remap|Value range|Remap the operator's output to a specified range, defaulting to [0, 1].|

## Example


![img](../../images/nodes/CloudRemapValues_hsd_example.png)

Corresponding Hesiod file: [CloudRemapValues.hsd](../../examples/CloudRemapValues.hsd). Use [Ctrl+I] in the node editor to import a hsd file within your current project.

!!! note
    Example files are kept up-to-date with the latest version of [Hesiod](https://github.com/otto-link/Hesiod).
    If you find an error, please [open an issue](https://github.com/otto-link/Hesiod/issues).

  
