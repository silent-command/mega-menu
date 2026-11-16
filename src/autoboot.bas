10 rem mega net collection dispatcher: see src/handoff.h
20 m=$ff87ff0:s=0
30 if peek(m)=77 and peek(m+1)=78 and peek(m+2)=67 then s=peek(m+3)
35 border 0:background 0:foreground 1:print chr$(147);:poke $d070,255:palette restore
40 if s=1 then new
50 poke m,77:poke m+1,78:poke m+2,67
70 if s<>2 then poke m+3,1:run "menu"
80 n$="":for i=4 to 13:c=peek(m+i):if c>0 then n$=n$+chr$(c)
90 next
100 poke m+3,3:run (n$)
