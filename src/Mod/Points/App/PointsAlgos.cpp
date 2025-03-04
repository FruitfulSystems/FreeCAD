/***************************************************************************
 *   Copyright (c) 2011 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
# ifdef FC_OS_LINUX
#  include <unistd.h>
# endif
# include <memory>
# include <sstream>
#endif

#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

#include <Base/Console.h>
#include <Base/Converter.h>
#include <Base/Exception.h>
#include <Base/FileInfo.h>
#include <Base/Sequencer.h>
#include <Base/Stream.h>
#include <E57Format.h>

#include "PointsAlgos.h"


using namespace Points;

void PointsAlgos::Load(PointKernel &points, const char *FileName)
{
    Base::FileInfo File(FileName);

    // checking on the file
    if (!File.isReadable())
        throw Base::FileException("File to load not existing or not readable", FileName);

    if (File.hasExtension("asc"))
        LoadAscii(points,FileName);
    else
        throw Base::RuntimeError("Unknown ending");
}

void PointsAlgos::LoadAscii(PointKernel &points, const char *FileName)
{
    boost::regex rx("^\\s*([-+]?[0-9]*)\\.?([0-9]+([eE][-+]?[0-9]+)?)"
                     "\\s+([-+]?[0-9]*)\\.?([0-9]+([eE][-+]?[0-9]+)?)"
                     "\\s+([-+]?[0-9]*)\\.?([0-9]+([eE][-+]?[0-9]+)?)\\s*$");
    //boost::regex rx("(\\b[0-9]+\\.([0-9]+\\b)?|\\.[0-9]+\\b)");
    //boost::regex rx("^\\s*(-?[0-9]*)\\.([0-9]+)\\s+(-?[0-9]*)\\.([0-9]+)\\s+(-?[0-9]*)\\.([0-9]+)\\s*$");
    boost::cmatch what;

    Base::Vector3d pt;
    int LineCnt=0;
    std::string line;
    Base::FileInfo fi(FileName);

    Base::ifstream tmp_str(fi, std::ios::in);

    // estimating size
    while (std::getline(tmp_str,line))
        LineCnt++;

    // resize the PointKernel
    points.resize(LineCnt);

    Base::SequencerLauncher seq( "Loading points...", LineCnt );

    // again to the beginning
    Base::ifstream file(fi, std::ios::in);
    LineCnt = 0;

    try {
        // read file
        while (std::getline(file, line)) {
            if (boost::regex_match(line.c_str(), what, rx)) {
                pt.x = std::atof(what[1].first);
                pt.y = std::atof(what[4].first);
                pt.z = std::atof(what[7].first);

                points.setPoint(LineCnt,pt);
                seq.next();
                LineCnt++;
            }
        }
    }
    catch (...) {
        points.clear();
        throw Base::BadFormatError("Reading in points failed.");
    }

    // now remove the last points from the kernel
    // Note: first we allocate memory corresponding to the number of lines (points and comments)
    //       and read in the file twice. But then the size of the kernel is too high
    if (LineCnt < (int)points.size())
        points.erase(LineCnt, points.size());
}

// ----------------------------------------------------------------------------

Reader::Reader()
{
    width = 0;
    height = 0;
}

Reader::~Reader()
{
}

void Reader::clear()
{
    intensity.clear();
    colors.clear();
    normals.clear();
}

const PointKernel& Reader::getPoints() const
{
    return points;
}

bool Reader::hasProperties() const
{
    return (hasIntensities() || hasColors() || hasNormals());
}

const std::vector<float>& Reader::getIntensities() const
{
    return intensity;
}

bool Reader::hasIntensities() const
{
    return (!intensity.empty());
}

const std::vector<App::Color>& Reader::getColors() const
{
    return colors;
}

bool Reader::hasColors() const
{
    return (!colors.empty());
}

const std::vector<Base::Vector3f>& Reader::getNormals() const
{
    return normals;
}

bool Reader::hasNormals() const
{
    return (!normals.empty());
}

bool Reader::isStructured() const
{
    return (width > 1 && height > 1);
}

int Reader::getWidth() const
{
    return width;
}

int Reader::getHeight() const
{
    return height;
}

// ----------------------------------------------------------------------------

AscReader::AscReader()
{
}

AscReader::~AscReader()
{
}

void AscReader::read(const std::string& filename)
{
    points.load(filename.c_str());
}

// ----------------------------------------------------------------------------

namespace Points {
class Converter {
public:
    Converter() = default;
    virtual ~Converter() = default;
    virtual std::string toString(double) const = 0;
    virtual double toDouble(Base::InputStream&) const = 0;
    virtual int getSizeOf() const = 0;

private:
    Converter(const Converter&) = delete;
    Converter(Converter&&) = delete;
    Converter& operator= (const Converter&) = delete;
    Converter& operator= (Converter&&) = delete;
};
template <typename T>
class ConverterT : public Converter {
public:
    std::string toString(double f) const override {
        T c = static_cast<T>(f);
        std::ostringstream oss;
        oss.precision(7);
        oss.setf(std::ostringstream::showpoint);
        oss << c;
        return oss.str();
    }
    double toDouble(Base::InputStream& str) const override {
        T c;
        str >> c;
        return static_cast<double>(c);
    }
    int getSizeOf() const override {
        return sizeof(T);
    }
};

using ConverterPtr = std::shared_ptr<Converter>;

class DataStreambuf : public std::streambuf
{
public:
    explicit DataStreambuf(const std::vector<char>& data) : _buffer(data) {
        _beg = 0;
        _end = data.size();
        _cur = 0;
    }
    ~DataStreambuf() override {
    }

protected:
    int_type uflow() override {
        if (_cur == _end)
            return traits_type::eof();

        return static_cast<DataStreambuf::int_type>(_buffer[_cur++]) & 0x000000ff;
    }
    int_type underflow() override {
        if (_cur == _end)
            return traits_type::eof();

        return static_cast<DataStreambuf::int_type>(_buffer[_cur]) & 0x000000ff;
    }
    int_type pbackfail(int_type ch) override {
        if (_cur == _beg || (ch != traits_type::eof() && ch != _buffer[_cur-1]))
            return traits_type::eof();

        return static_cast<DataStreambuf::int_type>(_buffer[--_cur]) & 0x000000ff;
    }
    std::streamsize showmanyc() override {
        return _end - _cur;
    }
    pos_type seekoff(std::streambuf::off_type off,
        std::ios_base::seekdir way,
        std::ios_base::openmode =
            std::ios::in | std::ios::out) override {
        int p_pos=-1;
        if (way == std::ios_base::beg)
            p_pos = _beg;
        else if (way == std::ios_base::end)
            p_pos = _end;
        else if (way == std::ios_base::cur)
            p_pos = _cur;

        if (p_pos > _end)
            return traits_type::eof();

        if (((p_pos + off) > _end) || ((p_pos + off) < _beg))
            return traits_type::eof();

        _cur = p_pos+ off;

        return ((p_pos+off) - _beg);
    }
    pos_type seekpos(std::streambuf::pos_type pos,
        std::ios_base::openmode which =
        std::ios::in | std::ios::out) override {
        (void)which;
        return seekoff(pos, std::ios_base::beg);
    }

private:
    DataStreambuf(const DataStreambuf&) = delete;
    DataStreambuf(DataStreambuf&&) = delete;
    DataStreambuf& operator=(const DataStreambuf&) = delete;
    DataStreambuf& operator=(DataStreambuf&&) = delete;

private:
    const std::vector<char>& _buffer;
    int _beg, _end, _cur;
};

//Taken from https://github.com/PointCloudLibrary/pcl/blob/master/io/src/lzf.cpp
unsigned int
lzfDecompress (const void *const in_data,  unsigned int in_len,
                    void             *out_data, unsigned int out_len)
{
  unsigned char const *ip = static_cast<const unsigned char *> (in_data);
  unsigned char       *op = static_cast<unsigned char *> (out_data);
  unsigned char const *const in_end  = ip + in_len;
  unsigned char       *const out_end = op + out_len;

  do
  {
    unsigned int ctrl = *ip++;

    // Literal run
    if (ctrl < (1 << 5))
    {
      ctrl++;

      if (op + ctrl > out_end)
      {
        errno = E2BIG;
        return (0);
      }

      // Check for overflow
      if (ip + ctrl > in_end)
      {
        errno = EINVAL;
        return (0);
      }
      switch (ctrl)
      {
      case 32: *op++ = *ip++;
          /* FALLTHRU */
      case 31: *op++ = *ip++;
          /* FALLTHRU */
      case 30: *op++ = *ip++;
          /* FALLTHRU */
      case 29: *op++ = *ip++;
          /* FALLTHRU */
      case 28: *op++ = *ip++;
          /* FALLTHRU */
      case 27: *op++ = *ip++;
          /* FALLTHRU */
      case 26: *op++ = *ip++;
          /* FALLTHRU */
      case 25: *op++ = *ip++;
          /* FALLTHRU */
      case 24: *op++ = *ip++;
          /* FALLTHRU */
      case 23: *op++ = *ip++;
          /* FALLTHRU */
      case 22: *op++ = *ip++;
          /* FALLTHRU */
      case 21: *op++ = *ip++;
          /* FALLTHRU */
      case 20: *op++ = *ip++;
          /* FALLTHRU */
      case 19: *op++ = *ip++;
          /* FALLTHRU */
      case 18: *op++ = *ip++;
          /* FALLTHRU */
      case 17: *op++ = *ip++;
          /* FALLTHRU */
      case 16: *op++ = *ip++;
          /* FALLTHRU */
      case 15: *op++ = *ip++;
          /* FALLTHRU */
      case 14: *op++ = *ip++;
          /* FALLTHRU */
      case 13: *op++ = *ip++;
          /* FALLTHRU */
      case 12: *op++ = *ip++;
          /* FALLTHRU */
      case 11: *op++ = *ip++;
          /* FALLTHRU */
      case 10: *op++ = *ip++;
          /* FALLTHRU */
      case  9: *op++ = *ip++;
          /* FALLTHRU */
      case  8: *op++ = *ip++;
          /* FALLTHRU */
      case  7: *op++ = *ip++;
          /* FALLTHRU */
      case  6: *op++ = *ip++;
          /* FALLTHRU */
      case  5: *op++ = *ip++;
          /* FALLTHRU */
      case  4: *op++ = *ip++;
          /* FALLTHRU */
      case  3: *op++ = *ip++;
          /* FALLTHRU */
      case  2: *op++ = *ip++;
          /* FALLTHRU */
      case  1: *op++ = *ip++;
      }
    }
    // Back reference
    else
    {
      unsigned int len = ctrl >> 5;

      unsigned char *ref = op - ((ctrl & 0x1f) << 8) - 1;

      // Check for overflow
      if (ip >= in_end)
      {
        errno = EINVAL;
        return (0);
      }
      if (len == 7)
      {
        len += *ip++;
        // Check for overflow
        if (ip >= in_end)
        {
          errno = EINVAL;
          return (0);
        }
      }
      ref -= *ip++;

      if (op + len + 2 > out_end)
      {
        errno = E2BIG;
        return (0);
      }

      if (ref < static_cast<unsigned char *> (out_data))
      {
        errno = EINVAL;
        return (0);
      }

      switch (len)
      {
        default:
        {
          len += 2;

          if (op >= ref + len)
          {
            // Disjunct areas
            memcpy (op, ref, len);
            op += len;
          }
          else
          {
            // Overlapping, use byte by byte copying
            do
              *op++ = *ref++;
            while (--len);
          }

          break;
        }
        case 9: *op++ = *ref++;
          /* FALLTHRU */
        case 8: *op++ = *ref++;
          /* FALLTHRU */
        case 7: *op++ = *ref++;
          /* FALLTHRU */
        case 6: *op++ = *ref++;
          /* FALLTHRU */
        case 5: *op++ = *ref++;
          /* FALLTHRU */
        case 4: *op++ = *ref++;
          /* FALLTHRU */
        case 3: *op++ = *ref++;
          /* FALLTHRU */
        case 2: *op++ = *ref++;
          /* FALLTHRU */
        case 1: *op++ = *ref++;
          /* FALLTHRU */
        case 0: *op++ = *ref++; // two octets more
                *op++ = *ref++;
      }
    }
  }
  while (ip < in_end);

  return (static_cast<unsigned int> (op - static_cast<unsigned char*> (out_data)));
}
}

PlyReader::PlyReader()
{
}

PlyReader::~PlyReader()
{
}

void PlyReader::read(const std::string& filename)
{
    clear();
    this->width = 1;
    this->height = 0;

    Base::FileInfo fi(filename);
    Base::ifstream inp(fi, std::ios::in | std::ios::binary);

    std::string format;
    std::vector<std::string> fields;
    std::vector<std::string> types;
    std::vector<int> sizes;
    std::size_t offset = 0;
    std::size_t numPoints = readHeader(inp, format, offset, fields, types, sizes);

    Eigen::MatrixXd data(numPoints, fields.size());
    if (format == "ascii") {
        readAscii(inp, offset, data);
    }
    else if (format == "binary_little_endian") {
        readBinary(false, inp, offset, types, sizes, data);
    }
    else if (format == "binary_big_endian") {
        readBinary(true, inp, offset, types, sizes, data);
    }

    std::vector<std::string>::iterator it;
    std::size_t max_size = std::numeric_limits<std::size_t>::max();

    // x field
    std::size_t x = max_size;
    it = std::find(fields.begin(), fields.end(), "x");
    if (it != fields.end())
        x = std::distance(fields.begin(), it);

    // y field
    std::size_t y = max_size;
    it = std::find(fields.begin(), fields.end(), "y");
    if (it != fields.end())
        y = std::distance(fields.begin(), it);

    // z field
    std::size_t z = max_size;
    it = std::find(fields.begin(), fields.end(), "z");
    if (it != fields.end())
        z = std::distance(fields.begin(), it);

    // normal x field
    std::size_t normal_x = max_size;
    it = std::find(fields.begin(), fields.end(), "normal_x");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "nx");
    if (it != fields.end())
        normal_x = std::distance(fields.begin(), it);

    // normal y field
    std::size_t normal_y = max_size;
    it = std::find(fields.begin(), fields.end(), "normal_y");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "ny");
    if (it != fields.end())
        normal_y = std::distance(fields.begin(), it);

    // normal z field
    std::size_t normal_z = max_size;
    it = std::find(fields.begin(), fields.end(), "normal_z");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "nz");
    if (it != fields.end())
        normal_z = std::distance(fields.begin(), it);

    // intensity field
    std::size_t greyvalue = max_size;
    it = std::find(fields.begin(), fields.end(), "intensity");
    if (it != fields.end())
        greyvalue = std::distance(fields.begin(), it);

    // rgb(a) field
    std::size_t red = max_size, green = max_size, blue = max_size, alpha = max_size;
    it = std::find(fields.begin(), fields.end(), "red");
    if (it != fields.end())
        red = std::distance(fields.begin(), it);

    it = std::find(fields.begin(), fields.end(), "green");
    if (it != fields.end())
        green = std::distance(fields.begin(), it);

    it = std::find(fields.begin(), fields.end(), "blue");
    if (it != fields.end())
        blue = std::distance(fields.begin(), it);

    it = std::find(fields.begin(), fields.end(), "alpha");
    if (it != fields.end())
        alpha = std::distance(fields.begin(), it);

    // transfer the data
    bool hasData = (x != max_size && y != max_size && z != max_size);
    bool hasNormal = (normal_x != max_size && normal_y != max_size && normal_z != max_size);
    bool hasIntensity = (greyvalue != max_size);
    bool hasColor = (red != max_size && green != max_size && blue != max_size);

    if (hasData) {
        points.reserve(numPoints);
        for (std::size_t i=0; i<numPoints; i++) {
            points.push_back(Base::Vector3d(data(i,x),data(i,y),data(i,z)));
        }
    }

    if (hasData && hasNormal) {
        normals.reserve(numPoints);
        for (std::size_t i=0; i<numPoints; i++) {
            normals.emplace_back(data(i,normal_x),data(i,normal_y),data(i,normal_z));
        }
    }

    if (hasData && hasIntensity) {
        intensity.reserve(numPoints);
        for (std::size_t i=0; i<numPoints; i++) {
            intensity.push_back(data(i,greyvalue));
        }
    }

    if (hasData && hasColor) {
        colors.reserve(numPoints);
        float a = 1.0;
        if (types[red] == "uchar") {
            for (std::size_t i=0; i<numPoints; i++) {
                float r = data(i, red);
                float g = data(i, green);
                float b = data(i, blue);
                if (alpha != max_size)
                    a = data(i, alpha);
                colors.emplace_back(static_cast<float>(r)/255.0f,
                                    static_cast<float>(g)/255.0f,
                                    static_cast<float>(b)/255.0f,
                                    static_cast<float>(a)/255.0f);
            }
        }
        else if (types[red] == "float") {
            for (std::size_t i=0; i<numPoints; i++) {
                float r = data(i, red);
                float g = data(i, green);
                float b = data(i, blue);
                if (alpha != max_size)
                    a = data(i, alpha);
                colors.emplace_back(r, g, b, a);
            }
        }
    }
}

std::size_t PlyReader::readHeader(std::istream& in,
                                  std::string& format,
                                  std::size_t& offset,
                                  std::vector<std::string>& fields,
                                  std::vector<std::string>& types,
                                  std::vector<int>& sizes)
{
    std::string line, element;
    std::vector<std::string> list;
    std::size_t numPoints = 0;
    // a pair of numbers of elements and the total size of the properties
    std::vector<std::pair<std::size_t, std::size_t> > count_props;

    // read in the first three characters
    char ply[3];
    in.read(ply, 3);
    in.ignore(1);
    if (!in || (ply[0] != 'p') || (ply[1] != 'l') || (ply[2] != 'y'))
        throw Base::BadFormatError("Not a ply file"); // wrong header

    while (std::getline(in, line)) {
        if (line.empty())
            continue;

        // since the file is loaded in binary mode we may get the CR at the end
        boost::trim(line);
        boost::split(list, line, boost::is_any_of ("\t\r "), boost::token_compress_on);

        std::istringstream str(line);
        str.imbue(std::locale::classic());

        std::string kw;
        str >> kw;
        if (kw == "format") {
            if (list.size() != 3) {
                throw Base::BadFormatError("Not a valid ply file");
            }

            std::string format_string = list[1];
            std::string version = list[2];

            if (format_string == "ascii") {
                format = format_string;
            }
            else if (format_string == "binary_big_endian") {
                format = format_string;
            }
            else if (format_string == "binary_little_endian") {
                format = format_string;
            }
            else {
                // wrong format version
                throw Base::BadFormatError("Wrong format version");
            }
            if (version != "1.0") {
                // wrong version
                throw Base::BadFormatError("Wrong version number");
            }
        }
        else if (kw == "element") {
            if (list.size() != 3) {
                throw Base::BadFormatError("Not a valid ply file");
            }

            std::string name = list[1];
            std::size_t count = boost::lexical_cast<std::size_t>(list[2]);
            if (name == "vertex") {
                element = name;
                numPoints = count;
            }
            else {
                // if another element than 'vertex' comes first then calculate the offset
                if (numPoints == 0) {
                    count_props.emplace_back(count, 0);
                }
                else {
                    // this happens for elements coming after 'vertex'
                    element.clear();
                }
            }
        }
        else if (kw == "property") {
            if (list.size() < 3) {
                throw Base::BadFormatError("Not a valid ply file");
            }

            std::string name = list.back();
            std::list<std::string> number;
            if (list[1] == "list") {
                number.insert(number.end(), list.begin(), list.end());
                number.pop_front(); // property
                number.pop_front(); // list
                number.pop_back();
            }
            else {
                number.push_back(list[1]);
            }

            for (std::list<std::string>::iterator it = number.begin(); it != number.end(); ++it) {
                int size = 0;
                if (*it == "char" || *it == "int8") {
                    size = 1;
                }
                else if (*it == "uchar" || *it == "uint8") {
                    size = 1;
                }
                else if (*it == "short" || *it == "int16") {
                    size = 2;
                }
                else if (*it == "ushort" || *it == "uint16") {
                    size = 2;
                }
                else if (*it == "int" || *it == "int32") {
                    size = 4;
                }
                else if (*it == "uint" || *it == "uint32") {
                    size = 4;
                }
                else if (*it == "float" || *it == "float32") {
                    size = 4;
                }
                else if (*it == "double" || *it == "float64") {
                    size = 8;
                }
                else {
                    // no valid number type
                    throw Base::BadFormatError("Not a valid number type");
                }

                if (element == "vertex") {
                    // store the property name and type
                    fields.push_back(name);
                    types.push_back(*it);
                    sizes.push_back(size);
                }
                else if (!count_props.empty()) {
                    count_props.back().second += size;
                }
            }
        }
        else if (kw == "end_header") {
            break;
        }
    }

    if (fields.size() != sizes.size() ||
        fields.size() != types.size()) {
        throw Base::BadFormatError("");
    }

    offset = 0;
    if (format == "ascii") {
        // just sum up the number of lines to ignore
        std::vector<std::pair<std::size_t, std::size_t> >::iterator it;
        for (it = count_props.begin(); it != count_props.end(); ++it) {
            offset += it->first;
        }
    }
    else {
        std::vector<std::pair<std::size_t, std::size_t> >::iterator it;
        for (it = count_props.begin(); it != count_props.end(); ++it) {
            offset += it->first * it->second;
        }
    }

    return numPoints;
}

void PlyReader::readAscii(std::istream& inp, std::size_t offset, Eigen::MatrixXd& data)
{
    std::string line;
    std::size_t row = 0;
    std::size_t numPoints = data.rows();
    std::size_t numFields = data.cols();
    std::vector<std::string> list;
    while (std::getline(inp, line) && row < numPoints) {
        if (line.empty())
            continue;

        if (offset > 0) {
            offset--;
            continue;
        }

        // since the file is loaded in binary mode we may get the CR at the end
        boost::trim(line);
        boost::split(list, line, boost::is_any_of ("\t\r "), boost::token_compress_on);

        std::istringstream str(line);

        for (std::size_t col = 0; col < list.size() && col < numFields; col++) {
            double value = boost::lexical_cast<double>(list[col]);
            data(row, col) = value;
        }

        ++row;
    }
}

void PlyReader::readBinary(bool swapByteOrder,
                           std::istream& inp,
                           std::size_t offset,
                           const std::vector<std::string>& types,
                           const std::vector<int>& sizes,
                           Eigen::MatrixXd& data)
{
    std::size_t numPoints = data.rows();
    std::size_t numFields = data.cols();

    int neededSize = 0;
    ConverterPtr convert_float32(new ConverterT<float>);
    ConverterPtr convert_float64(new ConverterT<double>);
    ConverterPtr convert_int8(new ConverterT<int8_t>);
    ConverterPtr convert_uint8(new ConverterT<uint8_t>);
    ConverterPtr convert_int16(new ConverterT<int16_t>);
    ConverterPtr convert_uint16(new ConverterT<uint16_t>);
    ConverterPtr convert_int32(new ConverterT<int32_t>);
    ConverterPtr convert_uint32(new ConverterT<uint32_t>);

    std::vector<ConverterPtr> converters;
    for (std::size_t j=0; j<numFields; j++) {
        std::string t = types[j];
        switch (sizes[j]) {
        case 1:
            if (t == "char" || t == "int8")
                converters.push_back(convert_int8);
            else if (t == "uchar" || t == "uint8")
                converters.push_back(convert_uint8);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        case 2:
            if (t == "short" || t == "int16")
                converters.push_back(convert_int16);
            else if (t == "ushort" || t == "uint16")
                converters.push_back(convert_uint16);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        case 4:
            if (t == "int" || t == "int32")
                converters.push_back(convert_int32);
            else if (t == "uint" || t == "uint32")
                converters.push_back(convert_uint32);
            else if (t == "float" || t == "float32")
                converters.push_back(convert_float32);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        case 8:
            if (t == "double" || t == "float64")
                converters.push_back(convert_float64);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        default:
            throw Base::BadFormatError("Unexpected type");
        }

        neededSize += converters.back()->getSizeOf();
    }

    std::streamoff ulSize = 0;
    std::streamoff ulCurr = 0;
    std::streambuf* buf = inp.rdbuf();
    if (buf) {
        ulCurr = buf->pubseekoff(static_cast<std::streamoff>(offset), std::ios::cur, std::ios::in);
        ulSize = buf->pubseekoff(0, std::ios::end, std::ios::in);
        buf->pubseekoff(ulCurr, std::ios::beg, std::ios::in);
        if (ulCurr + neededSize*static_cast<std::streamoff>(numPoints) > ulSize)
            throw Base::BadFormatError("File expects too many elements");
    }

    Base::InputStream str(inp);
    str.setByteOrder(swapByteOrder ? Base::Stream::BigEndian : Base::Stream::LittleEndian);
    for (std::size_t i=0; i<numPoints; i++) {
        for (std::size_t j=0; j<numFields; j++) {
            double value = converters[j]->toDouble(str);
            data(i, j) = value;
        }
    }
}

// ----------------------------------------------------------------------------

PcdReader::PcdReader()
{
}

PcdReader::~PcdReader()
{
}

void PcdReader::read(const std::string& filename)
{
    clear();
    this->width = -1;
    this->height = -1;

    Base::FileInfo fi(filename);
    Base::ifstream inp(fi, std::ios::in | std::ios::binary);

    std::string format;
    std::vector<std::string> fields;
    std::vector<std::string> types;
    std::vector<int> sizes;
    std::size_t numPoints = readHeader(inp, format, fields, types, sizes);

    Eigen::MatrixXd data(numPoints, fields.size());
    if (format == "ascii") {
        readAscii(inp, data);
    }
    else if (format == "binary") {
        readBinary(false, inp, types, sizes, data);
    }
    else if (format == "binary_compressed") {
        unsigned int c, u;
        Base::InputStream str(inp);
        str >> c >> u;

        std::vector<char> compressed(c);
        inp.read(&compressed[0], c);
        std::vector<char> uncompressed(u);
        if (lzfDecompress(&compressed[0], c, &uncompressed[0], u) == u) {
            DataStreambuf ibuf(uncompressed);
            std::istream istr(nullptr);
            istr.rdbuf(&ibuf);
            readBinary(true, istr, types, sizes, data);
        }
        else {
            throw Base::BadFormatError("Failed to decompress binary data");
        }
    }

    std::vector<std::string>::iterator it;
    std::size_t max_size = std::numeric_limits<std::size_t>::max();

    // x field
    std::size_t x = max_size;
    it = std::find(fields.begin(), fields.end(), "x");
    if (it != fields.end())
        x = std::distance(fields.begin(), it);

    // y field
    std::size_t y = max_size;
    it = std::find(fields.begin(), fields.end(), "y");
    if (it != fields.end())
        y = std::distance(fields.begin(), it);

    // z field
    std::size_t z = max_size;
    it = std::find(fields.begin(), fields.end(), "z");
    if (it != fields.end())
        z = std::distance(fields.begin(), it);

    // normal x field
    std::size_t normal_x = max_size;
    it = std::find(fields.begin(), fields.end(), "normal_x");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "nx");
    if (it != fields.end())
        normal_x = std::distance(fields.begin(), it);

    // normal y field
    std::size_t normal_y = max_size;
    it = std::find(fields.begin(), fields.end(), "normal_y");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "ny");
    if (it != fields.end())
        normal_y = std::distance(fields.begin(), it);

    // normal z field
    std::size_t normal_z = max_size;
    it = std::find(fields.begin(), fields.end(), "normal_z");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "nz");
    if (it != fields.end())
        normal_z = std::distance(fields.begin(), it);

    // intensity field
    std::size_t greyvalue = max_size;
    it = std::find(fields.begin(), fields.end(), "intensity");
    if (it != fields.end())
        greyvalue = std::distance(fields.begin(), it);

    // rgb(a) field
    std::size_t rgba = max_size;
    it = std::find(fields.begin(), fields.end(), "rgb");
    if (it == fields.end())
        it = std::find(fields.begin(), fields.end(), "rgba");
    if (it != fields.end())
        rgba = std::distance(fields.begin(), it);

    // transfer the data
    bool hasData = (x != max_size && y != max_size && z != max_size);
    bool hasNormal = (normal_x != max_size && normal_y != max_size && normal_z != max_size);
    bool hasIntensity = (greyvalue != max_size);
    bool hasColor = (rgba != max_size);

    if (hasData) {
        points.reserve(numPoints);
        for (std::size_t i=0; i<numPoints; i++) {
            points.push_back(Base::Vector3d(data(i,x),data(i,y),data(i,z)));
        }
    }

    if (hasData && hasNormal) {
        normals.reserve(numPoints);
        for (std::size_t i=0; i<numPoints; i++) {
            normals.emplace_back(data(i,normal_x),data(i,normal_y),data(i,normal_z));
        }
    }

    if (hasData && hasIntensity) {
        intensity.reserve(numPoints);
        for (std::size_t i=0; i<numPoints; i++) {
            intensity.push_back(data(i,greyvalue));
        }
    }

    if (hasData && hasColor) {
        colors.reserve(numPoints);
        if (types[rgba] == "U") {
            for (std::size_t i=0; i<numPoints; i++) {
                uint32_t packed = static_cast<uint32_t>(data(i,rgba));
                uint32_t a = (packed >> 24) & 0xff;
                uint32_t r = (packed >> 16) & 0xff;
                uint32_t g = (packed >> 8) & 0xff;
                uint32_t b = packed & 0xff;
                colors.emplace_back(static_cast<float>(r)/255.0f,
                                    static_cast<float>(g)/255.0f,
                                    static_cast<float>(b)/255.0f,
                                    static_cast<float>(a)/255.0f);
            }
        }
        else if (types[rgba] == "F") {
            static_assert(sizeof(float) == sizeof(uint32_t), "float and uint32_t have different sizes");
            for (std::size_t i=0; i<numPoints; i++) {
                float f = static_cast<float>(data(i,rgba));
                uint32_t packed;
                std::memcpy(&packed, &f, sizeof(packed));
                uint32_t a = (packed >> 24) & 0xff;
                uint32_t r = (packed >> 16) & 0xff;
                uint32_t g = (packed >> 8) & 0xff;
                uint32_t b = packed & 0xff;
                colors.emplace_back(static_cast<float>(r)/255.0f,
                                    static_cast<float>(g)/255.0f,
                                    static_cast<float>(b)/255.0f,
                                    static_cast<float>(a)/255.0f);
            }
        }
    }
}

std::size_t PcdReader::readHeader(std::istream& in,
                                  std::string& format,
                                  std::vector<std::string>& fields,
                                  std::vector<std::string>& types,
                                  std::vector<int>& sizes)
{
    std::string line;
    std::vector<std::string> counts;
    std::vector<std::string> list;
    std::size_t points = 0;

    while (std::getline(in, line)) {
        if (line.empty())
            continue;

        // since the file is loaded in binary mode we may get the CR at the end
        boost::trim(line);
        boost::split(list, line, boost::is_any_of ("\t\r "), boost::token_compress_on);

        std::istringstream str(line);
        str.imbue(std::locale::classic());

        std::string kw;
        str >> kw;
        if (kw == "FIELDS") {
            for (std::size_t i=1; i<list.size(); i++) {
                fields.push_back(list[i]);
            }
        }
        else if (kw == "SIZE") {
            for (std::size_t i=1; i<list.size(); i++) {
                sizes.push_back(boost::lexical_cast<int>(list[i]));
            }
        }
        else if (kw == "TYPE") {
            for (std::size_t i=1; i<list.size(); i++) {
                types.push_back(list[i]);
            }
        }
        else if (kw == "COUNT") {
            for (std::size_t i=1; i<list.size(); i++) {
                counts.push_back(list[i]);
            }
        }
        else if (kw == "WIDTH") {
            str >> std::ws >> this->width;
        }
        else if (kw == "HEIGHT") {
            str >> std::ws >> this->height;
        }
        else if (kw == "POINTS") {
            str >> std::ws >> points;
        }
        else if (kw == "DATA") {
            str >> std::ws >> format;
            break;
        }
    }

    std::size_t w = static_cast<std::size_t>(this->width);
    std::size_t h = static_cast<std::size_t>(this->height);
    std::size_t size = w * h;
    if (fields.size() != sizes.size() ||
        fields.size() != types.size() ||
        fields.size() != counts.size() ||
        points != size) {
        throw Base::BadFormatError("");
    }

    return points;
}

void PcdReader::readAscii(std::istream& inp, Eigen::MatrixXd& data)
{
    std::string line;
    std::size_t row = 0;
    std::size_t numPoints = data.rows();
    std::size_t numFields = data.cols();
    std::vector<std::string> list;
    while (std::getline(inp, line) && row < numPoints) {
        if (line.empty())
            continue;

        // since the file is loaded in binary mode we may get the CR at the end
        boost::trim(line);
        boost::split(list, line, boost::is_any_of ("\t\r "), boost::token_compress_on);

        std::istringstream str(line);

        for (std::size_t col = 0; col < list.size() && col < numFields; col++) {
            double value = boost::lexical_cast<double>(list[col]);
            data(row, col) = value;
        }

        ++row;
    }
}

void PcdReader::readBinary(bool transpose,
                           std::istream& inp,
                           const std::vector<std::string>& types,
                           const std::vector<int>& sizes,
                           Eigen::MatrixXd& data)
{
    std::size_t numPoints = data.rows();
    std::size_t numFields = data.cols();

    int neededSize = 0;
    ConverterPtr convert_float32(new ConverterT<float>);
    ConverterPtr convert_float64(new ConverterT<double>);
    ConverterPtr convert_int8(new ConverterT<int8_t>);
    ConverterPtr convert_uint8(new ConverterT<uint8_t>);
    ConverterPtr convert_int16(new ConverterT<int16_t>);
    ConverterPtr convert_uint16(new ConverterT<uint16_t>);
    ConverterPtr convert_int32(new ConverterT<int32_t>);
    ConverterPtr convert_uint32(new ConverterT<uint32_t>);

    std::vector<ConverterPtr> converters;
    for (std::size_t j=0; j<numFields; j++) {
        char t = types[j][0];
        switch (sizes[j]) {
        case 1:
            if (t == 'I')
                converters.push_back(convert_int8);
            else if (t == 'U')
                converters.push_back(convert_uint8);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        case 2:
            if (t == 'I')
                converters.push_back(convert_int16);
            else if (t == 'U')
                converters.push_back(convert_uint16);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        case 4:
            if (t == 'I')
                converters.push_back(convert_int32);
            else if (t == 'U')
                converters.push_back(convert_uint32);
            else if (t == 'F')
                converters.push_back(convert_float32);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        case 8:
            if (t == 'F')
                converters.push_back(convert_float64);
            else
                throw Base::BadFormatError("Unexpected type");
            break;
        default:
            throw Base::BadFormatError("Unexpected type");
        }

        neededSize += converters.back()->getSizeOf();
    }

    std::streamoff ulSize = 0;
    std::streamoff ulCurr = 0;
    std::streambuf* buf = inp.rdbuf();
    if (buf) {
        ulCurr = buf->pubseekoff(0, std::ios::cur, std::ios::in);
        ulSize = buf->pubseekoff(0, std::ios::end, std::ios::in);
        buf->pubseekoff(ulCurr, std::ios::beg, std::ios::in);
        if (ulCurr + neededSize*static_cast<std::streamoff>(numPoints) > ulSize)
            throw Base::BadFormatError("File expects too many elements");
    }

    Base::InputStream str(inp);
    if (transpose) {
        for (std::size_t j=0; j<numFields; j++) {
            for (std::size_t i=0; i<numPoints; i++) {
                double value = converters[j]->toDouble(str);
                data(i, j) = value;
            }
        }
    }
    else {
        for (std::size_t i=0; i<numPoints; i++) {
            for (std::size_t j=0; j<numFields; j++) {
                double value = converters[j]->toDouble(str);
                data(i, j) = value;
            }
        }
    }
}

// ----------------------------------------------------------------------------

E57Reader::E57Reader(const bool& Color, const bool& State, const float& Distance)
{
    useColor = Color;
    checkState = State;
    minDistance = Distance;
}

E57Reader::~E57Reader()
{
}

void E57Reader::read(const std::string& filename)
{
    try {
        // read file
        e57::ImageFile imfi(filename, "r");
        e57::StructureNode root = imfi.root();
        if (root.isDefined("data3D")) {
            e57::VectorNode data3D(root.get("data3D"));
            for (int child = 0; child < data3D.childCount(); ++child) {
                e57::StructureNode            scan_data(data3D.get(child));
                e57::CompressedVectorNode     cvn(scan_data.get("points"));
                e57::StructureNode            prototype(cvn.prototype());
                // create buffers for the compressed vector reader
                const size_t buf_size = 1024;
                double xyz[buf_size * 3];
                double intensity[buf_size];
                int64_t state[buf_size];
                unsigned rgb[buf_size * 3];
                int64_t nil[buf_size];

                // check the channels which are needed
                unsigned ptr_xyz[3];
                unsigned ptr_rgb[3];
                bool inty = false;
                bool inv_state = false;
                unsigned cnt_xyz = 0;
                unsigned cnt_rgb = 0;
                std::vector<e57::SourceDestBuffer> sdb;
                for (int i = 0; i < prototype.childCount(); ++i) {
                    e57::Node n(prototype.get(i));
                    if ((n.type() == e57::E57_FLOAT) || (n.type() == e57::E57_SCALED_INTEGER)) {
                        if (n.elementName() == "cartesianX") {
                            ptr_xyz[0] = cnt_xyz++;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(xyz[0])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else if (n.elementName() == "cartesianY") {
                            ptr_xyz[1] = cnt_xyz++;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(xyz[buf_size])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else if (n.elementName() == "cartesianZ") {
                            ptr_xyz[2] = cnt_xyz++;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(xyz[2 * buf_size])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else if (n.elementName() == "intensity") {
                            inty = true;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(intensity[0])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else {
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(nil[0])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }

                    }
                    else if (n.type() == e57::E57_INTEGER) {
                        if (n.elementName() == "colorRed") {
                            ptr_rgb[0] = cnt_rgb++;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(rgb[0])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else if (n.elementName() == "colorGreen") {
                            ptr_rgb[1] = cnt_rgb++;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(rgb[buf_size])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else if (n.elementName() == "colorBlue") {
                            ptr_rgb[2] = cnt_rgb++;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(rgb[2 * buf_size])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else if (n.elementName() == "cartesianInvalidState") {
                            inv_state = true;
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(state[0])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                        else {
                            sdb.emplace_back(
                                    imfi
                                    , n.elementName()
                                    , &(nil[0])
                                    , buf_size
                                    , true
                                    , true

                            );
                        }
                    }
                }

                // read the data

                if (cnt_xyz == 3) {
                    unsigned count;
                    unsigned cnt_pts = 0;
                    Base::Vector3d pt, last;
                    e57::CompressedVectorReader cvr(cvn.reader(sdb));
                    bool hasColor = (cnt_rgb == 3) && useColor;
                    bool hasState = inv_state && checkState;
                    bool filter = false;

                    while ((count = cvr.read())) {
                        for (size_t i = 0; i < count; ++i) {
                            filter = false;
                            if (hasState) {
                                if (state[i] != 0) { filter = true; }
                            }
                            pt.x = xyz[ptr_xyz[0] * buf_size + i];
                            pt.y = xyz[ptr_xyz[1] * buf_size + i];
                            pt.z = xyz[ptr_xyz[2] * buf_size + i];
                            if ((!filter) && (cnt_pts > 0)) {
                                if (Base::Distance(last, pt) < minDistance) {
                                    filter = true;
                                }
                            }
                            if (!filter) {
                                cnt_pts++;
                                points.push_back(pt);
                                last = pt;
                                if (hasColor) {
                                    App::Color c;
                                    c.r = static_cast<float>(rgb[ptr_rgb[0] * buf_size + i]) / 255.0f;
                                    c.g = static_cast<float>(rgb[ptr_rgb[1] * buf_size + i]) / 255.0f;
                                    c.b = static_cast<float>(rgb[ptr_rgb[2] * buf_size + i]) / 255.0f;
                                    if (inty) { c.a = intensity[i]; }
                                    colors.push_back(c);
                                }
                            }
                        }
                    }
                }
                else {
                    Base::Console().Error("Missing channels xyz.");
                }
            }
        }
    }
    catch (...) {
        points.clear();
        throw Base::BadFormatError("E57");
    }
}

// ----------------------------------------------------------------------------

Writer::Writer(const PointKernel& p) : points(p)
{
    width = p.size();
    height = 1;
}

Writer::~Writer()
{
}

void Writer::setIntensities(const std::vector<float>& i)
{
    intensity = i;
}

void Writer::setColors(const std::vector<App::Color>& c)
{
    colors = c;
}

void Writer::setNormals(const std::vector<Base::Vector3f>& n)
{
    normals = n;
}

void Writer::setWidth(int w)
{
    width = w;
}

void Writer::setHeight(int h)
{
    height = h;
}

void Writer::setPlacement(const Base::Placement& p)
{
    placement = p;
}

// ----------------------------------------------------------------------------

AscWriter::AscWriter(const PointKernel& p) : Writer(p)
{
}

AscWriter::~AscWriter()
{
}

void AscWriter::write(const std::string& filename)
{
    if (placement.isIdentity()) {
        points.save(filename.c_str());
    }
    else {
        PointKernel copy = points;
        copy.transformGeometry(placement.toMatrix());
        copy.save(filename.c_str());
    }
}

// ----------------------------------------------------------------------------

PlyWriter::PlyWriter(const PointKernel& p) : Writer(p)
{
}

PlyWriter::~PlyWriter()
{
}

void PlyWriter::write(const std::string& filename)
{
    std::list<std::string> properties;
    properties.emplace_back("float x");
    properties.emplace_back("float y");
    properties.emplace_back("float z");

    ConverterPtr convert_float(new ConverterT<float>);
    ConverterPtr convert_uint(new ConverterT<uint32_t>);

    std::vector<ConverterPtr> converters;
    converters.push_back(convert_float);
    converters.push_back(convert_float);
    converters.push_back(convert_float);

    bool hasIntensity = (intensity.size() == points.size());
    bool hasColors = (colors.size() == points.size());
    bool hasNormals = (normals.size() == points.size());

    if (hasNormals) {
        properties.emplace_back("float nx");
        properties.emplace_back("float ny");
        properties.emplace_back("float nz");
        converters.push_back(convert_float);
        converters.push_back(convert_float);
        converters.push_back(convert_float);
    }

    if (hasColors) {
        properties.emplace_back("uchar red");
        properties.emplace_back("uchar green");
        properties.emplace_back("uchar blue");
        properties.emplace_back("uchar alpha");
        converters.push_back(convert_uint);
        converters.push_back(convert_uint);
        converters.push_back(convert_uint);
        converters.push_back(convert_uint);
    }

    if (hasIntensity) {
        properties.emplace_back("float intensity");
        converters.push_back(convert_float);
    }

    std::size_t numPoints = points.size();
    std::size_t numValid = 0;
    const std::vector<Base::Vector3f>& pts = points.getBasicPoints();
    for (std::size_t i=0; i<numPoints; i++) {
        const Base::Vector3f& p = pts[i];
        if (!boost::math::isnan(p.x) &&
            !boost::math::isnan(p.y) &&
            !boost::math::isnan(p.z))
            numValid++;
    }

    Eigen::MatrixXf data(numPoints, properties.size());

    if (placement.isIdentity()) {
        for (std::size_t i=0; i<numPoints; i++) {
            data(i,0) = pts[i].x;
            data(i,1) = pts[i].y;
            data(i,2) = pts[i].z;
        }
    }
    else {
        Base::Vector3d tmp;
        for (std::size_t i=0; i<numPoints; i++) {
            tmp = Base::convertTo<Base::Vector3d>(pts[i]);
            placement.multVec(tmp, tmp);
            data(i,0) = static_cast<float>(tmp.x);
            data(i,1) = static_cast<float>(tmp.y);
            data(i,2) = static_cast<float>(tmp.z);
        }
    }

    std::size_t col = 3;
    if (hasNormals) {
        int col0 = col;
        int col1 = col+1;
        int col2 = col+2;
        Base::Rotation rot = placement.getRotation();
        if (rot.isIdentity()) {
            for (std::size_t i=0; i<numPoints; i++) {
                data(i,col0) = normals[i].x;
                data(i,col1) = normals[i].y;
                data(i,col2) = normals[i].z;
            }
        }
        else {
            Base::Vector3d tmp;
            for (std::size_t i=0; i<numPoints; i++) {
                tmp = Base::convertTo<Base::Vector3d>(normals[i]);
                rot.multVec(tmp, tmp);
                data(i,col0) = static_cast<float>(tmp.x);
                data(i,col1) = static_cast<float>(tmp.y);
                data(i,col2) = static_cast<float>(tmp.z);
            }
        }
        col += 3;
    }

    if (hasColors) {
        int col0 = col;
        int col1 = col+1;
        int col2 = col+2;
        int col3 = col+3;
        for (std::size_t i=0; i<numPoints; i++) {
            App::Color c = colors[i];
            data(i,col0) = (c.r*255.0f + 0.5f);
            data(i,col1) = (c.g*255.0f + 0.5f);
            data(i,col2) = (c.b*255.0f + 0.5f);
            data(i,col3) = (c.a*255.0f + 0.5f);
        }
        col += 4;
    }

    if (hasIntensity) {
        for (std::size_t i=0; i<numPoints; i++) {
            data(i,col) = intensity[i];
        }
        col += 1;
    }

    Base::ofstream out(filename, std::ios::out);
    out << "ply" << std::endl
        << "format ascii 1.0" << std::endl
        << "comment FreeCAD generated" << std::endl;
    out << "element vertex " << numValid << std::endl;

    // the properties
    for (std::list<std::string>::iterator it = properties.begin(); it != properties.end(); ++it)
        out << "property " << *it << std::endl;
    out << "end_header" << std::endl;

    for (std::size_t r=0; r<numPoints; r++) {
        if (boost::math::isnan(data(r,0)))
            continue;
        if (boost::math::isnan(data(r,1)))
            continue;
        if (boost::math::isnan(data(r,2)))
            continue;
        for (std::size_t c=0; c<col; c++) {
            float value = data(r,c);
            out << converters[c]->toString(value) << " ";
        }
        out << std::endl;
    }
}

// ----------------------------------------------------------------------------

PcdWriter::PcdWriter(const PointKernel& p) : Writer(p)
{
}

PcdWriter::~PcdWriter()
{
}

void PcdWriter::write(const std::string& filename)
{
    std::list<std::string> fields;
    fields.emplace_back("x");
    fields.emplace_back("y");
    fields.emplace_back("z");

    std::list<std::string> types;
    types.emplace_back("F");
    types.emplace_back("F");
    types.emplace_back("F");

    ConverterPtr convert_float(new ConverterT<float>);
    ConverterPtr convert_uint(new ConverterT<uint32_t>);

    std::vector<ConverterPtr> converters;
    converters.push_back(convert_float);
    converters.push_back(convert_float);
    converters.push_back(convert_float);

    bool hasIntensity = (intensity.size() == points.size());
    bool hasColors = (colors.size() == points.size());
    bool hasNormals = (normals.size() == points.size());

    if (hasNormals) {
        fields.emplace_back("normal_x");
        fields.emplace_back("normal_y");
        fields.emplace_back("normal_z");
        types.emplace_back("F");
        types.emplace_back("F");
        types.emplace_back("F");
        converters.push_back(convert_float);
        converters.push_back(convert_float);
        converters.push_back(convert_float);
    }

    if (hasColors) {
        fields.emplace_back("rgba");
        types.emplace_back("U");
        converters.push_back(convert_uint);
    }

    if (hasIntensity) {
        fields.emplace_back("intensity");
        types.emplace_back("F");
        converters.push_back(convert_float);
    }

    std::size_t numPoints = points.size();
    const std::vector<Base::Vector3f>& pts = points.getBasicPoints();

    Eigen::MatrixXd data(numPoints, fields.size());

    if (placement.isIdentity()) {
        for (std::size_t i=0; i<numPoints; i++) {
            data(i,0) = pts[i].x;
            data(i,1) = pts[i].y;
            data(i,2) = pts[i].z;
        }
    }
    else {
        Base::Vector3d tmp;
        for (std::size_t i=0; i<numPoints; i++) {
            tmp = Base::convertTo<Base::Vector3d>(pts[i]);
            placement.multVec(tmp, tmp);
            data(i,0) = static_cast<float>(tmp.x);
            data(i,1) = static_cast<float>(tmp.y);
            data(i,2) = static_cast<float>(tmp.z);
        }
    }

    std::size_t col = 3;
    if (hasNormals) {
        int col0 = col;
        int col1 = col+1;
        int col2 = col+2;
        Base::Rotation rot = placement.getRotation();
        if (rot.isIdentity()) {
            for (std::size_t i=0; i<numPoints; i++) {
                data(i,col0) = normals[i].x;
                data(i,col1) = normals[i].y;
                data(i,col2) = normals[i].z;
            }
        }
        else {
            Base::Vector3d tmp;
            for (std::size_t i=0; i<numPoints; i++) {
                tmp = Base::convertTo<Base::Vector3d>(normals[i]);
                rot.multVec(tmp, tmp);
                data(i,col0) = static_cast<float>(tmp.x);
                data(i,col1) = static_cast<float>(tmp.y);
                data(i,col2) = static_cast<float>(tmp.z);
            }
        }
        col += 3;
    }

    if (hasColors) {
        for (std::size_t i=0; i<numPoints; i++) {
            App::Color c = colors[i];
            // http://docs.pointclouds.org/1.3.0/structpcl_1_1_r_g_b.html
            uint32_t packed = static_cast<uint32_t>(c.a*255.0f + 0.5f) << 24 |
                              static_cast<uint32_t>(c.r*255.0f + 0.5f) << 16 |
                              static_cast<uint32_t>(c.g*255.0f + 0.5f) << 8  |
                              static_cast<uint32_t>(c.b*255.0f + 0.5f);

            data(i,col) = packed;
        }
        col += 1;
    }

    if (hasIntensity) {
        for (std::size_t i=0; i<numPoints; i++) {
            data(i,col) = intensity[i];
        }
        col += 1;
    }

    std::size_t numFields = fields.size();
    Base::ofstream out(filename, std::ios::out);
    out << "# .PCD v0.7 - Point Cloud Data file format" << std::endl
        << "VERSION 0.7" << std::endl;

    // the fields
    out << "FIELDS";
    for (std::list<std::string>::iterator it = fields.begin(); it != fields.end(); ++it)
        out << " " << *it;
    out << std::endl;

    // the sizes
    out << "SIZE";
    for (std::size_t i=0; i<numFields; i++)
        out << " 4";
    out << std::endl;

    // the types
    out << "TYPE";
    for (std::list<std::string>::iterator it = types.begin(); it != types.end(); ++it)
        out << " " << *it;
    out << std::endl;

    // the count
    out << "COUNT";
    for (std::size_t i=0; i<numFields; i++)
        out << " 1";
    out << std::endl;

    out << "WIDTH " << width << std::endl;
    out << "HEIGHT " << height << std::endl;

    Base::Placement plm;
    Base::Vector3d p = plm.getPosition();
    Base::Rotation o = plm.getRotation();
    double x,y,z,w; o.getValue(x,y,z,w);
    out << "VIEWPOINT " << p.x << " " << p.y << " " << p.z
        << " " << w << " " << x << " " << y << " " << z << std::endl;

    out << "POINTS " << numPoints << std::endl
        << "DATA ascii" << std::endl;

    for (std::size_t r=0; r<numPoints; r++) {
        for (std::size_t c=0; c<col; c++) {
            double value = data(r,c);
            if (boost::math::isnan(value))
                out << "nan ";
            else
                out << converters[c]->toString(value) << " ";
        }
        out << std::endl;
    }
}
