//---------------------------------------------------------------------------

#ifndef ConsoleH
#define ConsoleH
//---------------------------------------------------------------------------

class Console
{//Ra: klasa statyczna gromadz�ca sygna�y steruj�ce oraz informacje zwrotne
 //Ra: stan wej�cia zmieniany klawiatur� albo dedykowanym urz�dzeniem
 //Ra: stan wyj�cia zmieniany przez symulacj� (mierniki, kontrolki)
private:
 static int iMode; //tryb pracy
 static int iConfig; //dodatkowa informacja o sprz�cie (np. numer LPT)
 static int iBits; //podstawowy zestaw lampek
 static void __fastcall BitsUpdate(int mask);
public:
 __fastcall Console();
 __fastcall ~Console();
 static void __fastcall ModeSet(int m,int h=0);
 static void __fastcall BitsSet(int mask,int entry=0);
 static void __fastcall BitsClear(int mask,int entry=0);
 static void __fastcall On();
 static void __fastcall Off();
};

#endif
