#pragma once
#include <cstdint>

namespace Driver::IO::NVMe
{
    struct __attribute__((packed)) SubmissionQueueEntry
    {
        uint8_t opcode;
        uint8_t flags;
        uint16_t commandID;
        uint32_t NSID;
        uint64_t reserved;
        uint64_t MPTR;
        uint64_t PRP1;
        uint64_t PRP2;
        uint32_t CDW10;
        uint32_t CDW11, CDW12, CDW13, CDW14, CDW15;
    };

    struct __attribute__((packed)) CompletionQueueEntry
    {
        uint32_t result;
        uint32_t reserved;
        uint16_t sqHead;
        uint16_t sqID;
        uint16_t commandID;
        uint16_t status;
    };

    bool initialise();
}
