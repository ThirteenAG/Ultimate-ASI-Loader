d3d8to9
=======

"d3d8to9" is a pseudo-driver module that intends to improve compatibility and stability in games using Direct3D 8 for rendering by converting all API calls and lowlevel shaders to equivalent Direct3D 9 ones. By that it also opens those games to the new possibilities from proven tools and wrappers written for Direct3D 9.

This project is part of [ReShade](http://reshade.me), an advanced and generic post-processing injector, but now maintained as an open source standalone too, in hope for it being useful to the community.

## Contributing

You'll need Visual Studio 2013 or higher to open the project file and the standalone DirectX SDK, required for the D3DX headers and libraries used for disassembling and assembling the shaders.

Any contributions to the project are welcomed, it's recommended to use GitHub [pull requests](https://help.github.com/articles/using-pull-requests/).

## License

All the source code is licensed under the conditions of the [zlib license](LICENSE.txt).
