
#pragma once

class CF2smIntf
{
public:
    CF2smIntf();
	~CF2smIntf();

    void Init();
    void Close();
    void StartWrite(int seed);
    void StartRead();
    bool CheckResult();
    bool IsReady();
    void ResetErrCounter();

    int m_fd;
    void *virtual_base;
};
