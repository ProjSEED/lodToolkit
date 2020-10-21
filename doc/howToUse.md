# lodToolkit
- level-of-details toolkit(LTK)

## osgbTo3mx
- Convert *osgb* lod tree to Bentley ContextCapture *[3mx](https://docs.bentley.com/LiveContent/web/ContextCapture%20Help-v9/en/GUID-CED0ABE6-2EE3-458D-9810-D87EC3C521BD.html)* tree.

### How to use
```
osgbTo3mx.exe --input <DIR> --output <DIR>
	-i, --input, [required], input dir path
	-o, --output, [required], output dir path
```

### Example
```
osgbTo3mx.exe -i E:\Data\Test -o E:\Data\Test_3mx
```

### The input dir should look like this
```
--metadata.xml

--Data\Tile_000_000\Tile_000_000.osgb
```

## pointcloudToLod
- Convert point cloud in *ply/las/laz/xyz* format to *osgb/[3mx](https://docs.bentley.com/LiveContent/web/ContextCapture%20Help-v9/en/GUID-CED0ABE6-2EE3-458D-9810-D87EC3C521BD.html)* lod tree, so that the point cloud could be loaded instantly.
> This program could handle extremely large point cloud as *ply/las/laz/xyz* file is streaming to the convertor.

### How to use
```
pointcloudToLod.exe --input <FILE> --output <DIR>
	-i, --input, [required], input file path, <ply/las/laz/xyz>
	-o, --output, [required], output dir path
	-m, --mode, [optional, default=3mx], output mode, <3mx/osgb>
	-r, --lodRatio, [optional, default=1.0], use <1.0 value if original pointcloud is very sparse, use >1.0 value if original pointcloud is very dense
	-t, --tileSize, [optional, default=1000000], max number of point in one tile
	-n, --nodeSize, [optional, default=5000], max number of point in one node
	-d, --depth, [optional, default=99], max lod tree depth
	-p, --pointSize, [optional, default=10.0], point size
	-c, --colorMode, [optional, default=iHeightBlend], [las/lsz format only] <rgb/iGrey/iBlueWhiteRed/iHeightBlend>, iGrey/iBlueWhiteRed/iHeightBlend use intensity from las/laz
```

### Example
```
pointcloudToLod.exe -m 3mx -i E:\Data\test.las -o E:\Data\Test_3mx
pointcloudToLod.exe -m osgb -i E:\Data\test.las -o E:\Data\Test_osgb
```

## meshToLod (WIP)
- Convert mesh in *obj* format to *osgb/[3mx](https://docs.bentley.com/LiveContent/web/ContextCapture%20Help-v9/en/GUID-CED0ABE6-2EE3-458D-9810-D87EC3C521BD.html)* lod tree, so that the mesh could be loaded instantly.
> This program only support *obj* format mesh with group info, each group will be a tile in the lod tree.

### How to use
```
```

### Example
```
```

