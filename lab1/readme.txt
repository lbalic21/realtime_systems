- programski kod u upravljacka_petlja.c simulira sustav za rad u stvarnom vremenu koji svoje upravljanje ostvaruje
jednostavnom petljom u kojoj provjerava sve komponente sustava.
- pravljač je dio sustava koji upravlja ostalim komponentama (ulazima) sustava. On je simuliran dretvom 
void *upravljac(struct opisUlaza *u).
- struktura podataka opisUlaza sadrži sve parametre potrebne za opis stanja pojedinog ulaza.
- svaki ulaz u sustav simuliran je zasebnom dretvom void *ulaz(struct opisUlaza *u).
- prilikom pokretanja programa potrebno je navesti broj željenih ulaza u sustav te simulacija automatski generira tražen 
broj dretvi.
- detaljniji opis pojedinih dijelova koda dan je u obliku komentara u programskom kodu.