
#ifndef ECI_H
#define ECI_H

/*
 * Extended Channel Interpretation (ECI) protocol defined by
 * AIM Inc. ITS/04-001 International Technical Specification:
 * Extended Channel Interpretations.
 */

#define ECI_DEFAULT ECI_ISO_8859_1

enum eci {
	ECI_ISO_8859_1  =  3, // ISO/IEC 8859-1  Latin alphabet No. 1
	ECI_ISO_8859_2  =  4, // ISO/IEC 8859-2  Latin alphabet No. 2
	ECI_ISO_8859_3  =  5, // ISO/IEC 8859-3  Latin alphabet No. 3
	ECI_ISO_8859_4  =  6, // ISO/IEC 8859-4  Latin alphabet No. 4
	ECI_ISO_8859_5  =  7, // ISO/IEC 8859-5  Latin/Cyrillic alphabet
	ECI_ISO_8859_6  =  8, // ISO/IEC 8859-6  Latin/Arabic alphabet
	ECI_ISO_8859_7  =  9, // ISO/IEC 8859-7  Latin/Greek alphabet
	ECI_ISO_8859_8  = 10, // ISO/IEC 8859-8  Latin/Hebrew alphabet
	ECI_ISO_8859_9  = 11, // ISO/IEC 8859-9  Latin alphabet No. 5
	ECI_ISO_8859_10 = 12, // ISO/IEC 8859-10 Latin alphabet No. 6
	ECI_ISO_8859_11 = 13, // ISO/IEC 8859-11 Latin/Thai alphabet
	/* 14 reserved */
	ECI_ISO_8859_13 = 15, // ISO/IEC 8859-13 Latin alphabet No. 7 (Baltic Rim)
	ECI_ISO_8859_14 = 16, // ISO/IEC 8859-14 Latin alphabet No. 8 (Celtic)
	ECI_ISO_8859_15 = 17, // ISO/IEC 8859-15 Latin alphabet No. 9
	ECI_ISO_8859_16 = 18, // ISO/IEC 8859-16 Latin alphabet No. 10
	/* 19 reserved */
	ECI_SHIFT_JIS   = 20, // Shift JIS (JIS X 0208 Annex 1 + JIS X 0201)
	ECI_WIN_1250    = 21, // Windows 1250 Latin 2 (Central Europe)
	ECI_WIN_1251    = 22, // Windows 1251 Cyrillic
	ECI_WIN_1252    = 23, // Windows 1252 Latin 1
	ECI_WIN_1256    = 24, // Windows 1256 Arabic
	ECI_UTF16_BE    = 25, // ISO/IEC 10646 UCS-2 (High order byte first)
	ECI_UTF8        = 26, // ISO/IEC 10646 UTF-8
	ECI_US_ASCII    = 27, // ISO/IEC 646:1991 International Reference Version of ISO 7-bit coded character set
	ECI_BIG5        = 28, // Big 5 (Taiwan) Chinese Character Set
	ECI_GB18030     = 29, // GB (PRC) Chinese Character Set
	ECI_EUC_KR      = 30  // Korean Character Set
};

#endif

