# OpenXR API Layers GUI

This is a tool for:
- detecting and fixing common problems with OpenXR API layers
- viewing information about OpenXR API layers
- enabling, disabling, adding, or removing OpenXR API layers
- re-ordering OpenXR API layers

There is no general way to detect errors with OpenXR API layers, so this tool is only able to detect problems that it knows about. If it doesn't show any errors, that just means that you have no particularly common problems, not necessarily that everything is set up correctly.

## Getting Started

1. Download [the latest version](https://github.com/fredemmott/OpenXR-API-Layers-GUI/releases/latest)
2. Extract it somewhere handy
3. Run `OpenXR-API-Layers-GUI.exe` :)

While this project is designed to be easily portable, it currently only supports Windows - [contributions are very welcome :)](CONTRIBUTING.md).

## FAQ

### Why does OpenXR Explorer show the layers in a different order?

OpenXR Explorer v1.4 (latest as of 2024-04-04) and below show API layers in alphabetical order; OpenXR API Layers GUI instead shows API layers in their actual order.

## Getting Help

No help is available for this tool, as every previous request for help has been for help with a specific API layer or game, not the tool, and I am unable to offer support for other people's software.

- If you have a problem with a specific layer, or aren't sure which order things should be in, search for how to get help for the layers in question
- If you have a problem with a game but don't know which layer is the problem, try disabling all layers. If that fixes it, enable them one at a time until you have problems, then look up how to get help for the layers. If you're stuck, try the forums, Discord, or Reddit for the gaame or headset you're trying to use - whichever is more relevant to the problem.
- If the game or API layers are unsupported, you might want to try forums/Discord/reddit for the game or headset you're trying to use - whichever is more relevant to your problem. I do not support other people's software, even if it's no longer supported by the authors.

## Screenshots

Common errors are automatically detected:

![Lots of errors](docs/errors.png)

Most errors can be automatically fixed; the 'Fix Them!' button in the screenshot produces this:

![Mostly fixed](docs/errors-fixed.png)

Detailed information about OpenXR API layers is also shown:

![Name, description, exposed extensions](docs/details.png)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

The majority of this project is licensed under the MIT license (below).

The images in the 'icons' subfolder may not be reused or distributed outside of this product.

A separate license for these images may be purchased from https://glyphicons.com

------

Copyright (c) 2023 Fred Emmott.

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
