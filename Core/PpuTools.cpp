#include "stdafx.h"
#include "PpuTools.h"
#include "Ppu.h"
#include "DebugTypes.h"
#include "Console.h"
#include "BaseCartridge.h"
#include "MemoryManager.h"
#include "NotificationManager.h"

PpuTools::PpuTools(Console *console, Ppu *ppu)
{
	_console = console;
	_ppu = ppu;
}

uint8_t PpuTools::GetTilePixelColor(const uint8_t* ram, const uint32_t ramMask, const uint8_t bpp, const uint32_t pixelStart, const uint8_t shift)
{
	uint8_t color;
	if(bpp == 2) {
		color = (((ram[(pixelStart + 0) & ramMask] >> shift) & 0x01) << 0);
		color |= (((ram[(pixelStart + 1) & ramMask] >> shift) & 0x01) << 1);
	} else if(bpp == 4) {
		color = (((ram[(pixelStart + 0) & ramMask] >> shift) & 0x01) << 0);
		color |= (((ram[(pixelStart + 1) & ramMask] >> shift) & 0x01) << 1);
		color |= (((ram[(pixelStart + 16) & ramMask] >> shift) & 0x01) << 2);
		color |= (((ram[(pixelStart + 17) & ramMask] >> shift) & 0x01) << 3);
	} else if(bpp == 8) {
		color = (((ram[(pixelStart + 0) & ramMask] >> shift) & 0x01) << 0);
		color |= (((ram[(pixelStart + 1) & ramMask] >> shift) & 0x01) << 1);
		color |= (((ram[(pixelStart + 16) & ramMask] >> shift) & 0x01) << 2);
		color |= (((ram[(pixelStart + 17) & ramMask] >> shift) & 0x01) << 3);
		color |= (((ram[(pixelStart + 32) & ramMask] >> shift) & 0x01) << 4);
		color |= (((ram[(pixelStart + 33) & ramMask] >> shift) & 0x01) << 5);
		color |= (((ram[(pixelStart + 48) & ramMask] >> shift) & 0x01) << 6);
		color |= (((ram[(pixelStart + 49) & ramMask] >> shift) & 0x01) << 7);
	} else {
		throw std::runtime_error("unsupported bpp");
	}
	return color;
}

uint32_t PpuTools::ToArgb(uint16_t color)
{
	uint8_t b = (color >> 10) << 3;
	uint8_t g = ((color >> 5) & 0x1F) << 3;
	uint8_t r = (color & 0x1F) << 3;

	return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void PpuTools::BlendColors(uint8_t output[4], uint8_t input[4])
{
	uint8_t alpha = input[3] + 1;
	uint8_t invertedAlpha = 256 - input[3];
	output[0] = (uint8_t)((alpha * input[0] + invertedAlpha * output[0]) >> 8);
	output[1] = (uint8_t)((alpha * input[1] + invertedAlpha * output[1]) >> 8);
	output[2] = (uint8_t)((alpha * input[2] + invertedAlpha * output[2]) >> 8);
	output[3] = 0xFF;
}

uint32_t PpuTools::GetRgbPixelColor(uint8_t colorIndex, uint8_t palette, uint8_t bpp, bool directColorMode, uint16_t basePaletteOffset)
{
	uint16_t paletteColor;
	if(bpp == 8 && directColorMode) {
		paletteColor = (
			(((colorIndex & 0x07) | (palette & 0x01)) << 1) |
			(((colorIndex & 0x38) | ((palette & 0x02) << 1)) << 3) |
			(((colorIndex & 0xC0) | ((palette & 0x04) << 3)) << 7)
		);
	} else {
		uint8_t* cgram = _ppu->GetCgRam();
		uint16_t paletteRamOffset = basePaletteOffset + (palette * (1 << bpp) + colorIndex) * 2;
		paletteColor = cgram[paletteRamOffset] | (cgram[paletteRamOffset + 1] << 8);
	}
	return ToArgb(paletteColor);
}

void PpuTools::GetTileView(GetTileViewOptions options, uint32_t *outBuffer)
{
	uint8_t* ram;
	uint32_t ramMask;
	uint8_t bpp;

	bool directColor = false;
	switch(options.Format) {
		case TileFormat::Bpp2: bpp = 2; break;
		case TileFormat::Bpp4: bpp = 4; break;

		case TileFormat::DirectColor:
			directColor = true;
			bpp = 8;
			break;

		case TileFormat::Mode7:
			bpp = 16;
			break;

		case TileFormat::Mode7DirectColor:
			directColor = true;
			bpp = 16;
			break;

		default: bpp = 8; break;
	}

	switch(options.MemoryType) {
		default:
		case SnesMemoryType::VideoRam:
			ramMask = Ppu::VideoRamSize - 1;
			ram = _ppu->GetVideoRam();
			break;

		case SnesMemoryType::PrgRom:
			ramMask = _console->GetCartridge()->DebugGetPrgRomSize() - 1;
			ram = _console->GetCartridge()->DebugGetPrgRom();
			break;

		case SnesMemoryType::WorkRam:
			ramMask = MemoryManager::WorkRamSize - 1;
			ram = _console->GetMemoryManager()->DebugGetWorkRam();
			break;
	}

	uint8_t* cgram = _ppu->GetCgRam();
	int bytesPerTile = 64 * bpp / 8;
	int tileCount = 0x10000 / bytesPerTile;
	
	uint16_t bgColor = (cgram[1] << 8) | cgram[0];
	for(int i = 0; i < 512 * 512; i++) {
		outBuffer[i] = ToArgb(bgColor);
	}

	int rowCount = tileCount / options.Width;

	if(options.Format == TileFormat::Mode7 || options.Format == TileFormat::Mode7DirectColor) {
		for(int row = 0; row < rowCount; row++) {
			uint32_t baseOffset = row * bytesPerTile * options.Width + options.AddressOffset;

			for(int column = 0; column < options.Width; column++) {
				uint32_t addr = baseOffset + bytesPerTile * column;

				for(int y = 0; y < 8; y++) {
					uint32_t pixelStart = addr + y * 16;
					for(int x = 0; x < 8; x++) {
						uint8_t color = ram[(pixelStart + x * 2 + 1) & ramMask];

						if(color != 0) {
							uint32_t rgbColor;
							if(directColor) {
								rgbColor = ToArgb(((color & 0x07) << 2) | ((color & 0x38) << 4) | ((color & 0xC0) << 7));
							} else {
								rgbColor = GetRgbPixelColor(color, 0, 8, false, 0);
							}
							outBuffer[((row * 8) + y) * (options.Width * 8) + column * 8 + x] = rgbColor;
						}
					}
				}
			}
		}
	} else {
		for(int row = 0; row < rowCount; row++) {
			uint32_t baseOffset = row * bytesPerTile * options.Width + options.AddressOffset;

			for(int column = 0; column < options.Width; column++) {
				uint32_t addr = baseOffset + bytesPerTile * column;

				for(int y = 0; y < 8; y++) {
					uint32_t pixelStart = addr + y * 2;
					for(int x = 0; x < 8; x++) {
						uint8_t color = GetTilePixelColor(ram, ramMask, bpp, pixelStart, 7 - x);
						if(color != 0) {
							outBuffer[((row * 8) + y) * (options.Width * 8) + column * 8 + x] = GetRgbPixelColor(color, options.Palette, bpp, directColor, 0);
						}
					}
				}
			}
		}
	}

	if(options.ShowTileGrid) {
		constexpr uint32_t gridColor = 0xA0AAAAFF;
		for(int j = 0; j < rowCount * 8; j++) {
			for(int i = 0; i < options.Width * 8; i++) {
				if((i & 0x07) == 0x07 || (j & 0x07) == 0x07) {
					BlendColors((uint8_t*)&outBuffer[j*options.Width*8+i], (uint8_t*)&gridColor);
				}
			}
		}
	}
}

void PpuTools::GetTilemap(GetTilemapOptions options, uint32_t* outBuffer)
{
	static constexpr uint8_t layerBpp[8][4] = {
		{ 2,2,2,2 }, { 4,4,2,0 }, { 4,4,0,0 }, { 8,4,0,0 }, { 8,2,0,0 }, { 4,2,0,0 }, { 4,0,0,0 }, { 8,0,0,0 }
	};

	PpuState state = _ppu->GetState();
	options.BgMode = state.BgMode;

	uint16_t basePaletteOffset = 0;
	if(options.BgMode == 0) {
		basePaletteOffset = options.Layer * 64;
	}

	uint8_t *vram = _ppu->GetVideoRam();
	uint8_t *cgram = _ppu->GetCgRam();
	LayerConfig layer = state.Layers[options.Layer];

	uint16_t bgColor = (cgram[1] << 8) | cgram[0];
	for(int i = 0; i < 512 * 512; i++) {
		outBuffer[i] = ToArgb(bgColor);
	}

	uint8_t bpp = layerBpp[options.BgMode][options.Layer];
	if(bpp == 0) {
		return;
	}

	bool largeTileWidth = layer.LargeTiles || options.BgMode == 5 || options.BgMode == 6;
	bool largeTileHeight = layer.LargeTiles;

	for(int row = 0; row < (layer.DoubleHeight ? 64 : 32); row++) {
		uint16_t addrVerticalScrollingOffset = layer.DoubleHeight ? ((row & 0x20) << (layer.DoubleWidth ? 6 : 5)) : 0;
		uint16_t baseOffset = (layer.TilemapAddress >> 1) + addrVerticalScrollingOffset + ((row & 0x1F) << 5);

		for(int column = 0; column < (layer.DoubleWidth ? 64 : 32); column++) {
			uint16_t addr = (baseOffset + (column & 0x1F) + (layer.DoubleWidth ? ((column & 0x20) << 5) : 0)) << 1;

			bool vMirror = (vram[addr + 1] & 0x80) != 0;
			bool hMirror = (vram[addr + 1] & 0x40) != 0;

			uint16_t tileIndex = ((vram[addr + 1] & 0x03) << 8) | vram[addr];
			uint16_t tileStart = layer.ChrAddress + tileIndex * 8 * bpp;

			if(largeTileWidth || largeTileHeight) {
				tileIndex = (
					tileIndex +
					(largeTileHeight ? ((row & 0x01) ? (vMirror ? 0 : 16) : (vMirror ? 16 : 0)) : 0) +
					(largeTileWidth ? ((column & 0x01) ? (hMirror ? 0 : 1) : (hMirror ? 1 : 0)) : 0)
				) & 0x3FF;
			}

			for(int y = 0; y < 8; y++) {
				uint8_t yOffset = vMirror ? (7 - y) : y;
				uint16_t pixelStart = tileStart + yOffset * 2;

				for(int x = 0; x < 8; x++) {
					uint8_t shift = hMirror ? x : (7 - x);
					uint8_t color = GetTilePixelColor(vram, Ppu::VideoRamSize - 1, bpp, pixelStart, shift);
					if(color != 0) {
						uint8_t palette = bpp == 8 ? 0 : (vram[addr + 1] >> 2) & 0x07;
						outBuffer[((row * 8) + y) * 512 + column * 8 + x] = GetRgbPixelColor(color, palette, bpp, false, basePaletteOffset);
					}
				}
			}
		}
	}

	if(options.ShowTileGrid) {
		constexpr uint32_t gridColor = 0xA0AAAAFF;
		for(int i = 0; i < 512 * 512; i++) {
			if((i & 0x07) == 0x07 || (i & 0x0E00) == 0x0E00) {
				BlendColors((uint8_t*)&outBuffer[i], (uint8_t*)&gridColor);
			}
		}
	}

	if(options.ShowScrollOverlay) {
		constexpr uint32_t overlayColor = 0x40FFFFFF;
		for(int y = 0; y < 240; y++) {
			for(int x = 0; x < 256; x++) {
				int xPos = layer.HScroll + x;
				int yPos = layer.VScroll + y;
				
				xPos &= layer.DoubleWidth ? 0x1FF : 0xFF;
				yPos &= layer.DoubleHeight ? 0x1FF : 0xFF;

				if(x == 0 || y == 0 || x == 255 || y == 239) {
					outBuffer[(yPos << 9) | xPos] = 0xAFFFFFFF;
				} else {
					BlendColors((uint8_t*)&outBuffer[(yPos << 9) | xPos], (uint8_t*)&overlayColor);
				}
			}
		}
	}
}

void PpuTools::SetViewerUpdateTiming(uint32_t viewerId, uint16_t scanline, uint16_t cycle)
{
	//TODO Thread safety
	_updateTimings[viewerId] = (scanline << 16) | cycle;
}

void PpuTools::RemoveViewer(uint32_t viewerId)
{
	//TODO Thread safety
	_updateTimings.erase(viewerId);
}

void PpuTools::UpdateViewers(uint16_t scanline, uint16_t cycle)
{
	uint32_t currentCycle = (scanline << 16) | cycle;
	for(auto updateTiming : _updateTimings) {
		if(updateTiming.second == currentCycle) {
			_console->GetNotificationManager()->SendNotification(ConsoleNotificationType::ViewerRefresh, (void*)(uint64_t)updateTiming.first);
		}
	}
}