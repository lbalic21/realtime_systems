- programski kod u visedretvenost.c simulira sustav za rad u stvarnom vremenu koji svoje upravljanje ostvaruje
  dretvama namijenjenim za obradu pojedinih ulaza
- struktura podataka opisUlaza sadrži sve parametre potrebne za opis stanja pojedinog ulaza
- svaki ulaz u sustav simuliran je zasebnom dretvom void *ulaz(struct opisUlaza *u)
- prilikom pokretanja programa potrebno je navesti broj željenih ulaza u sustav te simulacija automatski generira tražen 
  broj dretvi te uz to generira po jednu upravljačku dretvu zaduženu za obradu pojedinog ulaza
- detaljniji opis pojedinih dijelova koda dan je u obliku komentara u programskom kodu
