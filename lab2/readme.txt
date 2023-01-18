- programski kod u periodicki_prekidi.c simulira sustav za rad u stvarnom vremenu koji svoje upravljanje ostvaruje
  prekidnom funkcijom koja periodički provjerava ulaze
- u prograu je korišteno statičko raspoređivanje dretvi što znači da se ulazi provjeravaju uvijek istim redoslijedom
- struktura podataka opisUlaza sadrži sve parametre potrebne za opis stanja pojedinog ulaza
- svaki ulaz u sustav simuliran je zasebnom dretvom void *ulaz(struct opisUlaza *u)
- prilikom pokretanja programa potrebno je navesti broj željenih ulaza u sustav te simulacija automatski generira tražen 
  broj dretvi
- detaljniji opis pojedinih dijelova koda dan je u obliku komentara u programskom kodu