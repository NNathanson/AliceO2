// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#ifndef ALICEO2_FOCAL_HCAL_MAPPER_H
#define ALICEO2_FOCAL_HCAL_MAPPER_H
#include <array>
#include <exception>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>

namespace o2::focal
{

class HCALMapper
{
 public:
  static constexpr std::size_t NCOLUMN = 8;
  static constexpr std::size_t NROW = 8;
  static constexpr std::size_t NCHANNELS = NCOLUMN * NROW;

  class PositionException : public std::exception
  {
   public:
    PositionException(unsigned int column, unsigned int row);
    ~PositionException() noexcept final = default;

    const char* what() const noexcept final { return mMessage.c_str(); }
    unsigned int getColumn() const noexcept { return mColumn; }
    unsigned int getRow() const noexcept { return mRow; }
    void print(std::ostream& stream) const;

   private:
    unsigned int mColumn = 0;
    unsigned int mRow = 0;
    std::string mMessage;
  };

  class ChannelIDException : public std::exception
  {
   public:
    explicit ChannelIDException(unsigned int channelID);
    ~ChannelIDException() noexcept final = default;

    const char* what() const noexcept final { return mMessage.c_str(); }
    unsigned int getChannelID() const noexcept { return mChannelID; }
    void print(std::ostream& stream) const;

   private:
    unsigned int mChannelID = 0;
    std::string mMessage;
  };

  class MappingFileException : public std::runtime_error
  {
   public:
    using std::runtime_error::runtime_error;
  };

  /// Default constructor does nothing. You must call loadMapping() before use.
  HCALMapper() = default;

  /// Convenience constructor: loads mapping immediately.
  explicit HCALMapper(const std::string& mappingFile);

  /// Load/replace mapping from file (8x8)
  void loadMapping(const std::string& mappingFile);

  /// channelID -> (col,row)
  std::tuple<unsigned int, unsigned int> getRowColFromChannelID(unsigned int channelID) const;
  unsigned int getRow(unsigned int channelID) const;
  unsigned int getColumn(unsigned int channelID) const;

  /// (col,row) -> channelID
  unsigned int getChannelID(unsigned int col, unsigned int row) const;

 private:
  using Coord = std::tuple<unsigned int, unsigned int>;

  void buildInverseMapping();

  // Forward mapping: (col,row) -> channelID
  std::array<std::array<unsigned int, NROW>, NCOLUMN> mMapping{};

  // Inverse mapping: channelID -> (col,row)
  std::unordered_map<unsigned int, Coord> mInverseMapping;

  bool mLoaded = false;
};

std::ostream& operator<<(std::ostream& stream, const HCALMapper::PositionException& except);
std::ostream& operator<<(std::ostream& stream, const HCALMapper::ChannelIDException& except);

} // namespace o2::focal

#endif // ALICEO2_FOCAL_HCAL_MAPPER_H