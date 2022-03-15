#include "CarlaRecorder.h"
#include "CarlaRecorderHelpers.h"
#include "DReyeVRRecorder.h"

void DReyeVRCustomActorRecorder::Read(std::ifstream &InFile)
{
    Data.Read(InFile);
}

void DReyeVRCustomActorRecorder::Write(std::ofstream &OutFile) const
{
    Data.Write(OutFile);
}

std::string DReyeVRCustomActorRecorder::Print() const
{
    std::ostringstream oss;
    oss << TCHAR_TO_UTF8(*Data.ToString());
    return oss.str();
}
//---------------------------------------------

void DReyeVRCustomActorRecorders::Clear(void)
{
    AllData.clear();
}

void DReyeVRCustomActorRecorders::Add(const DReyeVRCustomActorRecorder &NewData)
{
    AllData.push_back(NewData);
}

void DReyeVRCustomActorRecorders::Write(std::ofstream &OutFile)
{
    // write the packet id
    WriteValue<char>(OutFile, static_cast<char>(CarlaRecorderPacketId::DReyeVR));
    std::streampos PosStart = OutFile.tellp();

    // write a dummy packet size
    uint32_t Total = 0;
    WriteValue<uint32_t>(OutFile, Total);

    // write total records
    Total = AllData.size();
    WriteValue<uint16_t>(OutFile, Total);

    for (auto &Snapshot : AllData)
        Snapshot.Write(OutFile);

    // write the real packet size
    std::streampos PosEnd = OutFile.tellp();
    Total = PosEnd - PosStart - sizeof(uint32_t);
    OutFile.seekp(PosStart, std::ios::beg);
    WriteValue<uint32_t>(OutFile, Total);
    OutFile.seekp(PosEnd, std::ios::beg);
}