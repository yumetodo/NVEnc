﻿// -----------------------------------------------------------------------------------------
// NVEnc by rigaya
// -----------------------------------------------------------------------------------------
//
// The MIT License
//
// Copyright (c) 2014-2016 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#include <array>
#include "convert_csp.h"
#include "NVEncFilter.h"
#include "NVEncParam.h"
#pragma warning (push)
#pragma warning (disable: 4819)
#include "cuda_runtime.h"
#include "device_launch_parameters.h"
#pragma warning (pop)

template<typename T, typename Tfunc>
static NppStatus denoise_yv12(FrameInfo *pOutputFrame, const FrameInfo *pInputFrame, Tfunc funcGauss, NppiMaskSize masksize) {
    const double factorX = pOutputFrame->width / (double)pInputFrame->width;
    const double factorY = pOutputFrame->height / (double)pInputFrame->height;
    auto srcSize = nppisize(pInputFrame);
    auto dstSize = nppisize(pOutputFrame);
    NppiPoint srcOffset = { 0 };
    //Y
    NppStatus sts = funcGauss(
        (const T *)pInputFrame->ptr,
        pInputFrame->pitch, srcSize, srcOffset,
        (T *)pOutputFrame->ptr,
        pOutputFrame->pitch, dstSize, masksize, NPP_BORDER_REPLICATE);
    if (sts != NPP_SUCCESS) {
        return sts;
    }
    //U
    srcSize.width  >>= 1;
    srcSize.height >>= 1;
    dstSize.width  >>= 1;
    dstSize.height >>= 1;
    sts = funcGauss(
        (const T *)((const uint8_t *)pInputFrame->ptr + pInputFrame->pitch * pInputFrame->height),
        pInputFrame->pitch, srcSize, srcOffset,
        (T *)((uint8_t *)pOutputFrame->ptr + pOutputFrame->pitch * pOutputFrame->height),
        pOutputFrame->pitch, dstSize, masksize, NPP_BORDER_REPLICATE);
    if (sts != NPP_SUCCESS) {
        return sts;
    }
    //V
    sts = funcGauss(
        (const T *)((const uint8_t *)pInputFrame->ptr + pInputFrame->pitch * pInputFrame->height * 3 / 2),
        pInputFrame->pitch, srcSize, srcOffset,
        (T *)((uint8_t *)pOutputFrame->ptr + pOutputFrame->pitch * pOutputFrame->height * 3 / 2),
        pOutputFrame->pitch, dstSize, masksize, NPP_BORDER_REPLICATE);
    return NPP_SUCCESS;
}

RGY_ERR NVEncFilterDenoiseGauss::denoiseYV12(FrameInfo *pOutputFrame, const FrameInfo *pInputFrame) {
    RGY_ERR sts = RGY_ERR_NONE;
    if (m_pParam->frameOut.csp != m_pParam->frameIn.csp) {
        AddMessage(RGY_LOG_ERROR, _T("csp does not match.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    auto pGaussParam = std::dynamic_pointer_cast<NVEncFilterParamGaussDenoise>(m_pParam);
    if (!pGaussParam) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    const auto supportedCspYV12High = make_array<RGY_CSP>(RGY_CSP_YV12_09, RGY_CSP_YV12_10, RGY_CSP_YV12_12, RGY_CSP_YV12_14, RGY_CSP_YV12_16);
    NppStatus nppsts = NPP_SUCCESS;
    if (pGaussParam->frameIn.csp == RGY_CSP_YV12) {
        nppsts = denoise_yv12<Npp8u>(pOutputFrame, pInputFrame, nppiFilterGaussBorder_8u_C1R, pGaussParam->masksize);
        if (nppsts != NPP_SUCCESS) {
            AddMessage(RGY_LOG_ERROR, _T("failed to denoise: %d, %s.\n"), nppsts, char_to_tstring(_cudaGetErrorEnum(nppsts)).c_str());
            sts = RGY_ERR_UNKNOWN;
        }
    } else if (std::find(supportedCspYV12High.begin(), supportedCspYV12High.end(), pGaussParam->frameIn.csp) != supportedCspYV12High.end()) {
        nppsts = denoise_yv12<Npp16u>(pOutputFrame, pInputFrame, nppiFilterGaussBorder_16u_C1R, pGaussParam->masksize);
        if (nppsts != NPP_SUCCESS) {
            AddMessage(RGY_LOG_ERROR, _T("failed to denoise: %d, %s.\n"), nppsts, char_to_tstring(_cudaGetErrorEnum(nppsts)).c_str());
            sts = RGY_ERR_UNKNOWN;
        }
    } else {
        AddMessage(RGY_LOG_ERROR, _T("unsupported csp.\n"));
        sts = RGY_ERR_UNSUPPORTED;
    }
    return RGY_ERR_NONE;
}

template<typename T, typename Tfunc>
static NppStatus denoise_yuv444(FrameInfo *pOutputFrame, const FrameInfo *pInputFrame, Tfunc funcGauss, NppiMaskSize masksize) {
    const double factorX = pOutputFrame->width / (double)pInputFrame->width;
    const double factorY = pOutputFrame->height / (double)pInputFrame->height;
    auto srcSize = nppisize(pInputFrame);
    auto dstSize = nppisize(pOutputFrame);
    NppiPoint srcOffset = { 0 };
    //Y
    NppStatus sts = funcGauss(
        (const T *)pInputFrame->ptr,
        pInputFrame->pitch, srcSize, srcOffset,
        (T *)pOutputFrame->ptr,
        pOutputFrame->pitch, dstSize, masksize, NPP_BORDER_REPLICATE);
    if (sts != NPP_SUCCESS) {
        return sts;
    }
    //U
    sts = funcGauss(
        (const T *)((const uint8_t *)pInputFrame->ptr + pInputFrame->pitch * pInputFrame->height),
        pInputFrame->pitch, srcSize, srcOffset,
        (T *)((uint8_t *)pOutputFrame->ptr + pOutputFrame->pitch * pOutputFrame->height),
        pOutputFrame->pitch, dstSize, masksize, NPP_BORDER_REPLICATE);
    if (sts != NPP_SUCCESS) {
        return sts;
    }
    //V
    sts = funcGauss(
        (const T *)((const uint8_t *)pInputFrame->ptr + pInputFrame->pitch * pInputFrame->height * 2),
        pInputFrame->pitch, srcSize, srcOffset,
        (T *)((uint8_t *)pOutputFrame->ptr + pOutputFrame->pitch * pOutputFrame->height * 2),
        pOutputFrame->pitch, dstSize, masksize, NPP_BORDER_REPLICATE);
    return NPP_SUCCESS;
}

RGY_ERR NVEncFilterDenoiseGauss::denoiseYUV444(FrameInfo *pOutputFrame, const FrameInfo *pInputFrame) {
    RGY_ERR sts = RGY_ERR_NONE;
    if (m_pParam->frameOut.csp != m_pParam->frameIn.csp) {
        AddMessage(RGY_LOG_ERROR, _T("csp does not match.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    auto pGaussParam = std::dynamic_pointer_cast<NVEncFilterParamGaussDenoise>(m_pParam);
    if (!pGaussParam) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    const auto supportedCspYUV444High = make_array<RGY_CSP>(RGY_CSP_YUV444_09, RGY_CSP_YUV444_10, RGY_CSP_YUV444_12, RGY_CSP_YUV444_14, RGY_CSP_YUV444_16);
    NppStatus nppsts = NPP_SUCCESS;
    if (pGaussParam->frameIn.csp == RGY_CSP_YUV444) {
        nppsts = denoise_yuv444<Npp8u>(pOutputFrame, pInputFrame, nppiFilterGaussBorder_8u_C1R, pGaussParam->masksize);
        if (nppsts != NPP_SUCCESS) {
            AddMessage(RGY_LOG_ERROR, _T("failed to denoise: %d, %s.\n"), nppsts, char_to_tstring(_cudaGetErrorEnum(nppsts)).c_str());
            sts = RGY_ERR_UNKNOWN;
        }
    } else if (std::find(supportedCspYUV444High.begin(), supportedCspYUV444High.end(), pGaussParam->frameIn.csp) != supportedCspYUV444High.end()) {
        nppsts = denoise_yuv444<Npp16u>(pOutputFrame, pInputFrame, nppiFilterGaussBorder_16u_C1R, pGaussParam->masksize);
        if (nppsts != NPP_SUCCESS) {
            AddMessage(RGY_LOG_ERROR, _T("failed to denoise: %d, %s.\n"), nppsts, char_to_tstring(_cudaGetErrorEnum(nppsts)).c_str());
            sts = RGY_ERR_UNKNOWN;
        }
    } else {
        AddMessage(RGY_LOG_ERROR, _T("unsupported csp.\n"));
        sts = RGY_ERR_UNSUPPORTED;
    }
    return RGY_ERR_NONE;
}

NVEncFilterDenoiseGauss::NVEncFilterDenoiseGauss() : m_bInterlacedWarn(false) {
    m_sFilterName = _T("gauss");
}

NVEncFilterDenoiseGauss::~NVEncFilterDenoiseGauss() {
    close();
}

RGY_ERR NVEncFilterDenoiseGauss::init(shared_ptr<NVEncFilterParam> pParam, shared_ptr<RGYLog> pPrintMes) {
    RGY_ERR sts = RGY_ERR_NONE;
    m_pPrintMes = pPrintMes;
    auto pGaussParam = std::dynamic_pointer_cast<NVEncFilterParamGaussDenoise>(pParam);
    if (!pGaussParam) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter type.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    if (!check_if_nppi_dll_available()) {
        AddMessage(RGY_LOG_ERROR, _T("vpp-gauss requires \"%s\", not available on your system.\n"), NPPI_DLL_NAME_TSTR);
        return RGY_ERR_NOT_FOUND;
    }
    //パラメータチェック
    if (pGaussParam->frameOut.height <= 0 || pGaussParam->frameOut.width <= 0) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid parameter.\n"));
        return RGY_ERR_INVALID_PARAM;
    }

    auto cudaerr = AllocFrameBuf(pGaussParam->frameOut, 1);
    if (cudaerr != cudaSuccess) {
        AddMessage(RGY_LOG_ERROR, _T("failed to allocate memory: %s.\n"), char_to_tstring(cudaGetErrorName(cudaerr)).c_str());
        return RGY_ERR_MEMORY_ALLOC;
    }
    pGaussParam->frameOut.pitch = m_pFrameBuf[0]->frame.pitch;

    setFilterInfo(pParam->print());
    m_pParam = pParam;
    return sts;
}

tstring NVEncFilterParamGaussDenoise::print() const {
    return strsprintf(_T("denoise(gauss): mask size: %s"),
        get_chr_from_value(list_nppi_gauss, masksize));
}

RGY_ERR NVEncFilterDenoiseGauss::run_filter(const FrameInfo *pInputFrame, FrameInfo **ppOutputFrames, int *pOutputFrameNum, cudaStream_t stream) {
    RGY_ERR sts = RGY_ERR_NONE;

    if (pInputFrame->ptr == nullptr) {
        return sts;
    }

    *pOutputFrameNum = 1;
    if (ppOutputFrames[0] == nullptr) {
        auto pOutFrame = m_pFrameBuf[m_nFrameIdx].get();
        ppOutputFrames[0] = &pOutFrame->frame;
        m_nFrameIdx = (m_nFrameIdx + 1) % m_pFrameBuf.size();
    }
    ppOutputFrames[0]->picstruct = pInputFrame->picstruct;
    if (interlaced(*pInputFrame)) {
        return filter_as_interlaced_pair(pInputFrame, ppOutputFrames[0], cudaStreamDefault);
    }
    const auto memcpyKind = getCudaMemcpyKind(pInputFrame->deivce_mem, ppOutputFrames[0]->deivce_mem);
    if (memcpyKind != cudaMemcpyDeviceToDevice) {
        AddMessage(RGY_LOG_ERROR, _T("only supported on device memory.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    if (m_pParam->frameOut.csp != m_pParam->frameIn.csp) {
        AddMessage(RGY_LOG_ERROR, _T("csp does not match.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    const auto supportedCspYV12   = make_array<RGY_CSP>(RGY_CSP_YV12, RGY_CSP_YV12_09, RGY_CSP_YV12_10, RGY_CSP_YV12_12, RGY_CSP_YV12_14, RGY_CSP_YV12_16);
    const auto supportedCspYUV444 = make_array<RGY_CSP>(RGY_CSP_YUV444, RGY_CSP_YUV444_09, RGY_CSP_YUV444_10, RGY_CSP_YUV444_12, RGY_CSP_YUV444_14, RGY_CSP_YUV444_16);

    if (std::find(supportedCspYV12.begin(), supportedCspYV12.end(), m_pParam->frameIn.csp) != supportedCspYV12.end()) {
        sts = denoiseYV12(ppOutputFrames[0], pInputFrame);
    } else if (std::find(supportedCspYUV444.begin(), supportedCspYUV444.end(), m_pParam->frameIn.csp) != supportedCspYUV444.end()) {
        sts = denoiseYUV444(ppOutputFrames[0], pInputFrame);
    } else {
        AddMessage(RGY_LOG_ERROR, _T("unsupported csp.\n"));
        sts = RGY_ERR_UNSUPPORTED;
    }
    return sts;
}

void NVEncFilterDenoiseGauss::close() {
    m_pFrameBuf.clear();
    m_bInterlacedWarn = false;
}
