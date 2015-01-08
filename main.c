////////////////////////////////////////////////////////////////////////////////
//
//
// Copyright (C) 2004 Daniel 'Necr0Potenc3' Cavalcanti
//
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//
//	March 2nd, 2004 	-- started it, wrote FlexSearch and PatchEncryption
//	March 3rd, 2004 	-- Added SetBreakpoints, not much time now!!
//	March 4th, 2004 	-- I can't see them! But they are watchin me... yes!!
//	March 5th, 2004 	-- added "user friendly" output for uolog, now its ready
//						for public
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>

//function prototypes
LONG __stdcall ExceptionFilter(struct _EXCEPTION_POINTERS *ExceptionInfo);
DWORD FleXSearch(PBYTE src, PBYTE buf, DWORD src_size, DWORD buf_size, BYTE flex_byte, int which);
void PatchEncryption(DWORD base_address, int size);
void SetBreakpoints(DWORD base_address, int size);
void ShowPatchLine(void);

//X_bpx is the offset for the bpx
//X_regbuf is the register that holds the buffer
//X_reglen is the register that holds the len
int send_bpx=0;
int send_regbuf=0;
int send_reglen=0;
BYTE send_byte;
int recv_bpx=0;
int recv_regbuf=0;
int recv_reglen=0;
BYTE recv_byte;

/*
Olly organizes as: EAX ECX EDX EBX ESP EBP ESI EDI
UOLog: EAX=1, EBX=2, ECX=3, EDX=4, ESI=5, EDI=6, EBP=7
*/
#define FOLKE_EAX	1
#define FOLKE_EBX	2
#define FOLKE_ECX	3
#define FOLKE_EDX	4
#define FOLKE_ESI	5
#define FOLKE_EDI	6
#define FOLKE_EBP	7


/* WHO SUMMONS ME?!? - the evil dll has shouted in disgust */
BOOL APIENTRY DLLMain(HINSTANCE hDLLInst, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hDLLInst);
		}break;
        case DLL_PROCESS_DETACH:
		{
		}break;
        case DLL_THREAD_ATTACH:		break;
        case DLL_THREAD_DETACH:		break;
    }
    return TRUE;
}

void WINAPI HookClient(void)
{
	DWORD base_address = (DWORD)GetModuleHandle(NULL);
	PatchEncryption(base_address, 0x1fffff);

	SetBreakpoints(base_address, 0x1fffff);
	SetUnhandledExceptionFilter(ExceptionFilter);

	return;
}

DWORD FleXSearch(PBYTE src, PBYTE buf, DWORD src_size, DWORD buf_size, BYTE flex_byte, int which)
{
	int count=0;
	for(int i=(int)buf; i<buf_size+(int)buf; i++)
	{
		for(int j=0; j<src_size; j++)
		{
			//if its the last byte, it will cancel the loop
			//so I put it after the return
			if(src[j] == (BYTE)flex_byte && j != (src_size - 1))
				continue;
			else if(src[j] == (BYTE)flex_byte && j == (src_size - 1))
			{
				count++;
				if(count == which)
					return i;
			}

			//if there's a difference, stop checking
			if(src[j] != (*(PBYTE)(i+j)))
				break;

			if(j == (src_size - 1))
			{
				count++;
				if(count == which)
					return i;
			}
		}
	}

	//if it got this far, then couldnt find it
	return 0;
}

void PatchEncryption(DWORD base_address, int size)
{
	/* ENCRYPTION START */

	/*
	magic x90 encryption id: 81 f9 00 00 01 00 0f 8f
	patching : find the first 0f 84 and change it to 0f 85
	or first 0f 85 and change it to 0f 84
	*/
	BYTE crypt_id[8] = { 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x8F };
	DWORD crypt_addr = FleXSearch(crypt_id, (PBYTE)base_address, 8, size, 0xCC, 1);

	BYTE tst[200];
	//sprintf(tst, "%X", crypt_addr);
	//MessageBox(NULL, tst, "crypt_addr", 0);

	//search for the first JNZ or JNE from crypt_addr
	BYTE jnz_id[2] = { 0x0f, 0x85 };
	BYTE jne_id[2] = { 0x0f, 0x84 };
	DWORD jnz_addr = FleXSearch(jnz_id, (PBYTE)crypt_addr, 2, 0x50, 0xCC, 1);
	DWORD jne_addr = FleXSearch(jne_id, (PBYTE)crypt_addr, 2, 0x50, 0xCC, 1);

	//sprintf(tst, "%X %X", jnz_addr, jne_addr);
	//MessageBox(NULL, tst, "jnz and jne", 0);

	BYTE jne = 0x84;
	BYTE jnz = 0x85;
	if(!jne_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(jnz_addr+1), &jne, 1, 0);
	else if(!jnz_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(jne_addr+1), &jnz, 1, 0);
	else if(jnz_addr > jne_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(jne_addr+1), &jnz, 1, 0);
	else if(jne_addr > jnz_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(jnz_addr+1), &jne, 1, 0);

	/* ENCRYPTION END */

	/* DECRYPTION ONE START */

	/*
	this is a bitch...
	find the "dunno, select with invalid socket" string
	build a PUSH string
	find the 2nd reference to that push
	search for a XOR xxx CMP xxx JE xxx (33 DB 3B C3 0F 84) or TEST (85 C0)
	bellow the push, crack the CMP or TEST to CMP EAX, EAX
	*/
	//find the string
	BYTE dunno[34] = "dunno, select with invalid socket";
	DWORD dunno_pos = FleXSearch(dunno, (PBYTE)base_address, 34, size, 0xCC, 1);
	//sprintf(tst, "%X", dunno_pos);
	//MessageBox(NULL, tst, "dunno_pos", 0);

	//build a PUSH "dunno, select with invalid socket"
	BYTE dunno_push[5] = { 0x68, 0x00, 0x00, 0x00, 0x00 };
	memcpy(&dunno_push[1], &dunno_pos, 4);

	//find the 2nd reference to the PUSH
	DWORD dunno_ref = FleXSearch(dunno_push, (PBYTE)base_address, 5, size, 0xCC, 2);

	//sprintf(tst, "%X", dunno_ref);
	//MessageBox(NULL, tst, "dunno_ref", 0);

	//find the first XOR xxx CMP xxx JE xxx or TEST after
	//the 2nd reference to PUSH "dunno, select with invalid socket"
	BYTE xor_id[6] = { 0x33, 0xDB, 0x3B, 0xC3, 0x0F, 0x84 };
	BYTE test_id[2] = { 0x85, 0xC0 };
	DWORD xor_addr = FleXSearch(xor_id, (PBYTE)dunno_ref, 6, 0x30, 0xCC, 1);
	DWORD test_addr = FleXSearch(test_id, (PBYTE)dunno_ref, 2, 0x30, 0xCC, 1);

	//sprintf(tst, "%X %X", xor_addr, test_addr);
	//MessageBox(NULL, tst, "xor test", 0);


	/*
	case XOR: crack the CMP EAX, EBX (3B C3) to CMP EAX, EAX (3B C0), I could just
	nop them (90 90) but that takes more clock
	*/
	/*
	case TEST: crack the TEST EAX, EAX (85 C0) to CMP EAX, EAX (3B C0)
	*/
	BYTE cmp[2] = { 0x3B, 0xC0 };
	if(!test_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(xor_addr+2), &cmp, 2, 0);
	else if(!xor_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(test_addr), &cmp, 2, 0);
	else if(xor_addr > test_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(test_addr), &cmp, 2, 0);
	else if(test_addr > xor_addr)
		WriteProcessMemory(GetCurrentProcess(), (void*)(xor_addr+2), &cmp, 2, 0);

	/* DECRYPT ONE END */

	/* DECRYPT TWO START (now for the easy part) */

	/*
	search for 4A 83 CA F0 42 8A 94 32
	and above it, 85 xx 74 xx 33 xx 85 xx 7E xx
	the first TEST (85 xx) must be cracked to CMP EAX, EAX (3B C0)
	if I want to do it like LB does in UORice, I'd crack
	the first CMP xx JMP xx (85 xx 74 xx) to CMP EAX, EAX (3B C0)
	which is bellow the one I crack
	*/

	//find 4A 83 CA F0 42 8A 94 32
	BYTE newdecrypt_id[8] = { 0x4A, 0x83, 0xCA, 0xF0, 0x42, 0x8A, 0x94, 0x32 };
	DWORD newdecrypt_addr = FleXSearch(newdecrypt_id, (PBYTE)base_address, 8, size, 0xCC, 1);

	//sprintf(tst, "%X", newdecrypt_addr);
	//MessageBox(NULL, tst, "newdecrypt", 0);

	//find the TEST above it (not the one right above that is)
	//hm, the first flex search heh
	if(newdecrypt_addr)
	{
		BYTE dectest_id[10] = { 0x85, 0xCC, 0x74, 0xCC, 0x33, 0xCC, 0x85, 0xCC, 0x7E, 0xCC };
		DWORD dectest_addr = FleXSearch(dectest_id, (PBYTE)(newdecrypt_addr-0x100), 10, newdecrypt_addr, 0xCC, 1);
		//sprintf(tst, "%X", dectest_addr);
		//MessageBox(NULL, tst, "dectest_addr", 0);
		WriteProcessMemory(GetCurrentProcess(), (void*)(dectest_addr), &cmp, 2, 0);
	}

	return;
}

void SetBreakpoints(DWORD base_address, int size)
{
	/*
	compilers (uo clients are built using visual studio) use
	filling bytes for the void between functions
	I'm not sure if the minimum is 2 bytes (0x90) that's why I used
	RETN NOP, and searched it down, safer this way
	*/
	BYTE bpx = { 0xCC };

	BYTE crypt_id[8] = { 0x81, 0xF9, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x8F };
	DWORD crypt_addr = FleXSearch(crypt_id, (PBYTE)base_address, 8, size, 0xCC, 1);

	send_byte = (*(PBYTE)(crypt_addr));
	send_bpx = crypt_addr;
	WriteProcessMemory(GetCurrentProcess(), (void*)(send_bpx), &bpx, 1, 0);

	/* send bpx is set */

	/*
	find the bytes that identify the recv function
	then start looking 0x40 bytes above for something like NOP NOP NOT-NOP
	not-nop is the start of the function
	*/
	BYTE recv_id[9] = { 0x8B, 0x44, 0x24, 0x0c, 0x80, 0x38, 0x33, 0x0F, 0x85 };
	DWORD recv_addr = FleXSearch(recv_id, (PBYTE)base_address, 8, size, 0xCC, 1);

	int recvfunc_addr=0;
	BOOL has_nop=0;
	for(int i=recv_addr-0x40; i< recv_addr; i++)
	{
		if((*(PBYTE)(i)) == 0x90 && !has_nop)
		{
			has_nop=1;
			continue;
		}
		if((*(PBYTE)(i)) != 0x90 && has_nop)
		{
			recvfunc_addr=i;
			break;
		}
	}

	recv_byte = (*(PBYTE)(recvfunc_addr));
	recv_bpx = recvfunc_addr;
	WriteProcessMemory(GetCurrentProcess(), (void*)(recv_bpx), &bpx, 1, 0);

	BYTE tst[200];
	sprintf(tst, "%X %X", send_bpx, recv_bpx);
	MessageBox(NULL, tst, "send recv", 0);

	return;
}


/*
TODO: maybe it's not necessary since this is correct already, but the BPX of send and recv are setted
in the wrong position. those are the right offsets for UOLog, but in order to get the correct regbuf
and reglen, the bpx should be set at the end, so far this hasn't been a problem though.
newer clients (such as 4.0.2a) have two registers with the same reglen, but only one is the correct
usually its the last one in Olly list, this helped me not to re-write the setbpx function :)
*/
LONG __stdcall ExceptionFilter(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	//BYTE tst[200];
	//sprintf(tst, "%X %X %X", ExceptionInfo->ContextRecord->Eip, send_bpx, recv_bpx);
	//MessageBox(NULL, tst, "eip, send, recv", 0);
	if(ExceptionInfo->ContextRecord->Eip == send_bpx)
	{
		//if there is more than one reg with 0x80, then get the last one
		//probability is higher that the correct offset is the last
		if((*(PBYTE)ExceptionInfo->ContextRecord->Eax) == (BYTE)0x80)
			send_regbuf=FOLKE_EAX;

		if((*(PBYTE)ExceptionInfo->ContextRecord->Ecx) == (BYTE)0x80)
			send_regbuf=FOLKE_ECX;

		if((*(PBYTE)ExceptionInfo->ContextRecord->Edx) == (BYTE)0x80)
			send_regbuf=FOLKE_EDX;

		if((*(PBYTE)ExceptionInfo->ContextRecord->Ebx) == (BYTE)0x80)
			send_regbuf=FOLKE_EBX;

		if((*(PBYTE)ExceptionInfo->ContextRecord->Ebp) == (BYTE)0x80)
			send_regbuf=FOLKE_EBP;

		if((*(PBYTE)ExceptionInfo->ContextRecord->Esi) == (BYTE)0x80)
			send_regbuf=FOLKE_ESI;

		if((*(PBYTE)ExceptionInfo->ContextRecord->Edi) == (BYTE)0x80)
			send_regbuf=FOLKE_EDI;

		//now proceed to reglen, same thing as the above actually
		if((BYTE)(ExceptionInfo->ContextRecord->Eax&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_EAX;

		if((BYTE)(ExceptionInfo->ContextRecord->Ecx&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_ECX;

		if((BYTE)(ExceptionInfo->ContextRecord->Edx&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_EDX;

		if((BYTE)(ExceptionInfo->ContextRecord->Ebx&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_EBX;

		if((BYTE)(ExceptionInfo->ContextRecord->Ebp&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_EBP;

		if((BYTE)(ExceptionInfo->ContextRecord->Esi&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_ESI;

		if((BYTE)(ExceptionInfo->ContextRecord->Edi&0xff) == (BYTE)0x3E)
			send_reglen=FOLKE_EDI;

		//restore the byte at send and keep running the uo client
		WriteProcessMemory(GetCurrentProcess(), (void*)(send_bpx), &send_byte, 1, 0);
		BYTE tst[200];
		sprintf(tst, "%d %d", send_regbuf, send_reglen);
		MessageBox(NULL, tst, "send", 0);
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	if(ExceptionInfo->ContextRecord->Eip == recv_bpx)
	{
		//len for the 0x82 packet
		int len=0x02;
		//game servers list or login failed
		if((*(PBYTE)ExceptionInfo->ContextRecord->Eax) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Eax) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Eax) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Eax+1))&0xff;

				recv_regbuf=FOLKE_EAX;
			}

		if((*(PBYTE)ExceptionInfo->ContextRecord->Ecx) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Ecx) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Ecx) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Ecx+1))&0xff;

				recv_regbuf=FOLKE_ECX;
			}

		if((*(PBYTE)ExceptionInfo->ContextRecord->Edx) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Edx) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Edx) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Edx+1))&0xff;

				recv_regbuf=FOLKE_EDX;
			}

		if((*(PBYTE)ExceptionInfo->ContextRecord->Ebx) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Ebx) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Ebx) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Ebx+1))&0xff;

				recv_regbuf=FOLKE_EDX;
			}

		if((*(PBYTE)ExceptionInfo->ContextRecord->Ebp) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Ebp) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Ebp) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Ebp+1))&0xff;

				recv_regbuf=FOLKE_EBP;
			}

		if((*(PBYTE)ExceptionInfo->ContextRecord->Esi) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Esi) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Esi) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Esi+1))&0xff;

				recv_regbuf=FOLKE_ESI;
			}

		if((*(PBYTE)ExceptionInfo->ContextRecord->Edi) == (BYTE)0xa8
			|| (*(PBYTE)ExceptionInfo->ContextRecord->Edi) == (BYTE)0x82)
			{
				if((*(PBYTE)ExceptionInfo->ContextRecord->Edi) == (BYTE)0xa8)
					len = (*(PBYTE)(ExceptionInfo->ContextRecord->Edi+1))&0xff;

				recv_regbuf=FOLKE_EDI;
			}


		//now proceed to reglen, same thing as the above actually
		if((BYTE)(ExceptionInfo->ContextRecord->Eax&0xff) == len)
			recv_reglen=FOLKE_EAX;

		if((BYTE)(ExceptionInfo->ContextRecord->Ecx&0xff) == len)
			recv_reglen=FOLKE_ECX;

		if((BYTE)(ExceptionInfo->ContextRecord->Edx&0xff) == len)
			recv_reglen=FOLKE_EDX;

		if((BYTE)(ExceptionInfo->ContextRecord->Ebx&0xff) == len)
			recv_reglen=FOLKE_EBX;

		if((BYTE)(ExceptionInfo->ContextRecord->Ebp&0xff) == len)
			recv_reglen=FOLKE_EBP;

		if((BYTE)(ExceptionInfo->ContextRecord->Esi&0xff) == len)
			recv_reglen=FOLKE_ESI;

		if((BYTE)(ExceptionInfo->ContextRecord->Edi&0xff) == len)
			recv_reglen=FOLKE_EDI;

			BYTE tst[200];
		sprintf(tst, "%d %d", recv_regbuf, recv_reglen);
		MessageBox(NULL, tst, "recv", 0);

		ShowPatchLine();
		//restore the byte at send and keep running the uo client
		WriteProcessMemory(GetCurrentProcess(), (void*)(recv_bpx), &recv_byte, 1, 0);
		return EXCEPTION_CONTINUE_EXECUTION;
	}


	ExceptionInfo->ContextRecord->Eip += 1;
    return EXCEPTION_CONTINUE_EXECUTION;
}

void ShowPatchLine(void)
{
	DWORD base_address = (DWORD)GetModuleHandle(NULL);
	IMAGE_DOS_HEADER *doshdr=(IMAGE_DOS_HEADER*)base_address;
	IMAGE_FILE_HEADER *filehdr=(IMAGE_FILE_HEADER*)(base_address+doshdr->e_lfanew+sizeof(IMAGE_NT_SIGNATURE));
	DWORD datestamp = filehdr->TimeDateStamp;

	BYTE tst[200];
	sprintf(tst, "%d", datestamp);
	MessageBox(NULL, tst, "datestamp", 0);

	BYTE client_name[200];
	DWORD name_addr = FleXSearch("1.25.", (PBYTE)base_address, 5, 0x1fffff, 0xCC, 1);
	if(!name_addr)
		name_addr = FleXSearch("1.26.", (PBYTE)base_address, 5, 0x1fffff, 0xCC, 1);
	if(!name_addr)
		name_addr = FleXSearch("2.0.", (PBYTE)base_address, 4, 0x1fffff, 0xCC, 1);

	sprintf(tst, "%d", name_addr);
	MessageBox(NULL, tst, "name addr", 0);

	if(name_addr)
		strcpy(client_name, (void*)name_addr);
	else
	{
		DWORD version_addr = FleXSearch("%d.%d.%d%s", (PBYTE)base_address, 11, 0x1fffff, 0xCC, 1);
		BYTE version_push[5] = { 0x68, 0x00, 0x00, 0x00, 0x00 };
		memcpy(&version_push[1], &version_addr, 4);
		DWORD push_addr = FleXSearch(version_push, (PBYTE)base_address, 5, 0x1fffff, 0xCC, 1);
		int first_id = (*(PBYTE)(push_addr-1))&0xff;
		int middle_id = (*(PBYTE)(push_addr-3))&0xff;
		int third_id = (*(PBYTE)(push_addr-5))&0xff;
		PBYTE str_id;
		memcpy(&str_id, (void*)(push_addr-10), 4);

		sprintf(client_name, "%d.%d.%d%s", first_id, middle_id, third_id, str_id);
	}

	BYTE patchline[4096];
	sprintf(patchline, "%X: \"%s\"\t\t%X %d %d %X %d %d ;n0p3 sniffy / date Not-Tested", datestamp, client_name, send_bpx, send_regbuf, send_reglen, recv_bpx, recv_regbuf, recv_reglen);
	MessageBox(NULL, patchline, "patchline for UOLog.cfg", 0);

	return;
}
