// DF-READ2 Ico game data extractor
// Based on the http://astrange.ithinksw.net/ico/
//              http://astrange.ithinksw.net/ico/df-read.c
// read DFDATAS/data.df from ICO
// require little endian PC because the ICO game data are stored in little endian format

#include <cstdint>
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>
#include <iostream>
#include <filesystem>

extern "C" {
#include "zip.h"
}
typedef unsigned char ubyte;

using namespace std;
namespace filesystem = std::filesystem;

struct RootFileInfo {
  char filename[32];
  uint32_t offset;
  uint32_t size;
};

static_assert(sizeof(RootFileInfo) == 40, "Size of RootFileInfo must be 40bytes");

const int DF_FILENAME_LEN  = 0x200;

struct DfHeader {
  uint32_t file_count;
  uint32_t offset;
  uint32_t unknown[2];
};

typedef struct DfFileInfo {
  uint32_t unknown_value1;   // usually 30,31,32,33, ...?
  uint32_t unknown_value2;   // tim image 37, texture FF, ...?
  char unknown_header[4];
  uint32_t size;
  char filename[DF_FILENAME_LEN];
  uint32_t global_id;
  uint32_t offset;
  char unknown[12];
//  char reserved[8];
} DfFileInfo;

static_assert(sizeof(DfFileInfo) == 0x224, "");

void unpack_df(ifstream &input, const string &dst_dir)
{
  DfHeader header;
  // read header
  input.read(reinterpret_cast<char *>(&header), sizeof(DfHeader));

  vector<DfFileInfo> file_table(header.file_count);
  input.read(reinterpret_cast<char *>(file_table.data()), header.file_count * sizeof(DfFileInfo));
  vector<char> buffer;
  // extract files
  for (const DfFileInfo &info : file_table) {
    cout << "Unpacking file " << info.filename << endl;
    filesystem::path filename(info.filename);
    filesystem::path dst_filename(dst_dir);
    dst_filename /= filename;
    filesystem::path directory = dst_filename.parent_path();

    buffer.resize(info.size);
    input.seekg(info.offset, ios_base::beg);
    filesystem::create_directories(directory);
    input.read(buffer.data(), info.size);

    if (!input) {
      cerr << "Can't read file " << info.filename << endl;
      return;
    }

    ofstream output(dst_filename.string().c_str(), ios_base::out | ios_base::binary | ios_base::ate);
    output.write(buffer.data(), buffer.size());
  }
}

typedef struct IHContext {
  ubyte *buf;
  size_t size;
  size_t used;
} ih_context;

static long inflate_callback(char *into, long requested, void *v)
{
  ih_context *c = reinterpret_cast<IHContext *>(v);
  size_t to_read = requested;

  if (c->size == c->used) return 0;
  if ((c->used + requested) > c->size) to_read = c->size - c->used;

  memcpy(into, c->buf + c->used, to_read);

  c->used += to_read;

  return to_read;
}

void extract_file(const string &filename, size_t offset, size_t size, ifstream &file, const std::string &dst_dir)
{
  assert(!filename.empty());
  assert(offset > 0);
  assert(size > 0);
  cout << "Extracting file " << filename << endl;

  vector<uint8_t> buffer(size);
  // seek&read
  if (!file.seekg(offset, ios_base::beg)) {
    cerr << "Can't extract " << filename << "!" << endl;
    file.clear(); // clear error flag
    return;
  }
 
 
  if (!file.read(reinterpret_cast<char *>(buffer.data()), buffer.size())) {
    cerr << "Error while extracting " << filename << ", extracted file is incomplete!" << endl;
    streamsize bytes_read = file.gcount();
    file.clear(); // clear error flag

    if (bytes_read > 0)
      buffer.resize(file.gcount());
    else
      return;
  }
  cout << "Wanted " << buffer.size() << " got " << file.gcount() << endl;

  filesystem::path dst_path(dst_dir);
  dst_path /= filename;
  ofstream output(dst_path.string().c_str(), ios_base::binary | ios_base::out | ios_base::ate);

  if (buffer[0] == 0xEC) { // extract data
    vector<char> output_buffer(size);
    ih_context c = IHContext{ buffer.data(), size, 0 };
    InflateHandler ih = open_inflate_handler(inflate_callback, &c);
    long decompressed_bytes;

    while ((decompressed_bytes = zip_inflate(ih, output_buffer.data(), output_buffer.size())) > 0)
      output.write(output_buffer.data(), decompressed_bytes);

    close_inflate_handler(ih);
  } else { // write raw data
    if (!output.write(reinterpret_cast<char *>(buffer.data()), buffer.size())) {
      cerr << "Can't write file " << filename << endl;
      return;
    }
  }
}

void unpack_datafile(ifstream &input, const string &dst_dir)
{
  uint32_t file_count;

  // read first 4 bytes -> number of stored files, usually 172
  input.read(reinterpret_cast<char *>(&file_count), 4);

  if (file_count == 0) {
    cerr << "The datafile doesn't containt any data." << endl;
    return;
  }

  // read file table
  vector<RootFileInfo> file_table(file_count);

  input.read(reinterpret_cast<char *>(file_table.data()), file_table.size() * sizeof(RootFileInfo));

  for (const RootFileInfo &info : file_table) {
    //cout << "Filename " << info.filename << " offset " << info.offset << " size " << info.size << endl;
    
    extract_file(info.filename, info.offset, info.size, input, dst_dir);
    filesystem::path dst_filename(dst_dir);
    dst_filename /= info.filename;
    // extract files whith DF extension
    if (dst_filename.extension() == ".DF") { // extract DF archive
      ifstream df_input(dst_filename.string().c_str(), ios_base::in | ios_base::binary);

      if (!df_input) {
        cerr << "Can't open " << info.filename << endl;
        return;
      }
      unpack_df(df_input, dst_dir);
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    cout << "give DATA.DF as first argument" << endl;
    return -1;
  }

  ifstream input(argv[1], ios_base::in | ios_base::binary);

  if (!input) {
    cerr << "Can't open input file " << argv[1] << endl;
    return -1;
  }

  string dst_dir("data/");

  // ensure that destination directory exists
  filesystem::create_directories(filesystem::path(dst_dir));

  unpack_datafile(input, dst_dir);
	return 0;
}
