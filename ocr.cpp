#include "xerect.h"
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include <cstring>
#include <iostream>

#pragma pack(1)
struct BMPFileHeader {
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
};

struct BMPInfoHeader {
  uint32_t biSize;
  int32_t biWidth;
  int32_t biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t biXPelsPerMeter;
  int32_t biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
};
#pragma pack()

unsigned char AdjustContrast(unsigned char value, float contrastFactor) {
  float newValue = (((float)value / 255.0f - 0.5f) * contrastFactor + 0.5f) * 255.0f;
  if (newValue < 0.0f) newValue = 0.0f;
  if (newValue > 255.0f) newValue = 255.0f;
  return static_cast<unsigned char>(newValue);
}


void ConvertToBMP(const unsigned char *data, size_t size, int width, int height,
                  std::vector<unsigned char> &bmpData, float contrastFactor) {
  int paddingSize = (4 - (width * 3) % 4) % 4;

  BMPFileHeader fileHeader;
  fileHeader.bfType = 0x4D42;
  fileHeader.bfSize = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) +
                      (width * 3 + paddingSize) * height;
  fileHeader.bfReserved1 = 0;
  fileHeader.bfReserved2 = 0;
  fileHeader.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);

  BMPInfoHeader infoHeader;
  infoHeader.biSize = sizeof(BMPInfoHeader);
  infoHeader.biWidth = width;
  infoHeader.biHeight = height;
  infoHeader.biPlanes = 1;
  infoHeader.biBitCount = 24;
  infoHeader.biCompression = 0;
  infoHeader.biSizeImage = (width * 3 + paddingSize) * height;
  infoHeader.biXPelsPerMeter = 0;
  infoHeader.biYPelsPerMeter = 0;
  infoHeader.biClrUsed = 0;
  infoHeader.biClrImportant = 0;

  bmpData.resize(fileHeader.bfSize);

  std::memcpy(bmpData.data(), &fileHeader, sizeof(BMPFileHeader));
  std::memcpy(bmpData.data() + sizeof(BMPFileHeader), &infoHeader,
              sizeof(BMPInfoHeader));

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      size_t bmpIndex = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) +
                        (x * 3 + y * (width * 3 + paddingSize));
      size_t dataIndex = (x + (height - 1 - y) * width) * 3;

      // gray scaled param 
      unsigned char gray = static_cast<unsigned char>(
          0.299f * data[dataIndex + 2] + 
          0.587f * data[dataIndex + 1] + 
          0.114f * data[dataIndex]);

      gray = AdjustContrast(gray, contrastFactor);

      bmpData[bmpIndex] = gray;      // B
      bmpData[bmpIndex + 1] = gray;  // G
      bmpData[bmpIndex + 2] = gray;  // R
    }

    // padding
    std::memset(bmpData.data() + sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) +
                    width * 3 + y * (width * 3 + paddingSize),
                0, paddingSize);
  }
}


struct ProgramArgs {
  std::string lang;       // Tesseractの言語設定
  float contrastFactor;    // コントラスト調整の係数
};

bool ParseArguments(int argc, char *argv[], ProgramArgs &args) {
  // default value
  args.lang = "jpn";
  args.contrastFactor = 1.6f;

  // parse
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--lang") == 0 && i + 1 < argc) {
      args.lang = argv[i + 1]; 
      i++;  // skip next
    } else if (strcmp(argv[i], "--contrast") == 0 && i + 1 < argc) {
      args.contrastFactor = std::stof(argv[i + 1]);
      i++;  // skip next
    } else {
      std::cerr << "Unknown argument: " << argv[i] << std::endl;
      return false;  // error
    }
  }
  return true;
}


int main(int argc, char *argv[]) {
  char *outText;
  std::vector<unsigned char> bmpData;
  tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
  ProgramArgs args;
  // parse args
  if (!ParseArguments(argc, argv, args)) {
    std::cerr << "Usage: " << argv[0] << " [--lang <language>] [--contrast <factor>]" << std::endl;
    return 1;
  }

  // Tesseract API 初期化
  if (api->Init(NULL, args.lang.c_str())) {
    fprintf(stderr, "Could not initialize tesseract with language: %s\n", args.lang.c_str());
    return 1;
  }


  // open input image with leptonica library
  RetData glb_data = doGlabScreen();
  ConvertToBMP(glb_data.data, glb_data.size, glb_data.w, glb_data.h, bmpData, 1.6);
  free(glb_data.data);
  Pix *image = pixReadMemBmp(bmpData.data(), bmpData.size());

  api->SetImage(image);
  // get OCR result
  outText = api->GetUTF8Text();
  std::cout<< outText;

  // destroy used object and release memory
  api->End();
  delete api;
  delete[] outText;
  pixDestroy(&image);

  return 0;
}

