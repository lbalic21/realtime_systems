#include "stdio.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "sys/wait.h"
#include "unistd.h"

/*  Struktura podataka koja u sebi sadrži sve potrebne opisnike za upravljanje pojedinim ulazom */
struct opisUlaza
{
    int identifikator;
    int prvaPojava;
    int perioda;
    int stanje;
    clock_t trenutakZadnjePromjeneStanja;
    int zadnjiOdgovor;
    clock_t trenutakZadnjegOdgovora;
    int brojPromjenaStanja;
    int brojPrekasnihOdgovora;
    u_int64_t sumaKasnjenjaOdgovora;
    int maxVrijemeOdgovora;
    int K;
};

/*  zastavica koja označava aktivnost simulacije    */
static bool simulationRunning = true;

/*  broj ulaza (dretvi) kojima je potrebno upravljati   */
static int BROJ_ULAZA = 0;

/*  Simulacija trajanja obrade pojedinog ulaza.
    Trajanje obrade:
        - 30 ms (20% slučajeva)
        - 50 ms (50% slučajeva)
        - 80 ms (20% slučajeva)
        - 120 ms (10% slučajeva)
*/
void simulirajTrajanjeObrade(void)
{
    struct timespec t, t2;
    t.tv_sec = 0;
    int num = rand() % 100 + 1;
    if (num <= 20)
        t.tv_nsec = 30 * 1000000;
    else if (num > 20 && num <= 70)
        t.tv_nsec = 50 * 1000000;
    else if (num > 70 && num < 90)
        t.tv_nsec = 80 * 1000000;
    else
        t.tv_nsec = 120 * 1000000;

    nanosleep(&t, &t2);
}

/*  Upravljačka dretva  */
void *upravljac(struct opisUlaza *u)
{
    printf("Pokrenut upravljač!\n");
    int zadnjeStanje[BROJ_ULAZA];   //polje kojim upravljačka dretva pamti prošla stanja ulaza
    for(int i = 0; i < BROJ_ULAZA; i++) zadnjeStanje[i] = 0;              
    while (simulationRunning)
    {
        for (int i = 0; i < BROJ_ULAZA; i++)    //petlja koja prolazi svim ulazima
        {
            int stanje = (u + i)->stanje;
            if (stanje != zadnjeStanje[i])                  //ako je došlo do promjene stanja ulazne dretve, obradi ulaz, postavi odgovor(identičan trenutnom stanju)                                  
            {                                               //te upiši vrijeme postavljanja odgovora
                zadnjeStanje[i] = stanje;
                printf("Upravljac -> obrada ulaza %d    (%ld)\n", i + 1, clock());
                simulirajTrajanjeObrade();
                u[i].zadnjiOdgovor = stanje;
                u[i].trenutakZadnjegOdgovora = clock();
            }
        }
    }
    printf("Završio upravljač\n");
}

/*  Dretva koja simulira pojedini ulaz. U sustavu ih je pokrenuto točno BROJ_ULAZA. */
void *ulaz(struct opisUlaza *u)
{    
    struct timespec t, t2;
    t.tv_sec = u->prvaPojava;
    t.tv_nsec = 0;
    printf("Pokrenut ulaz %d, čekam prvu pojavu promjene stanja     (%ld)\n", u->identifikator, clock());
    nanosleep(&t, &t2);                 //spavanje dok ne prođe vrijeme postavljeno u strukturi opisa ulaza (vrijeme prvog postavljanja ulaza)

    u->stanje = rand() % 900 + 100;             //upis prve promjene stanja
    u->trenutakZadnjePromjeneStanja = clock();  //upis vremena prve promjene stanja
    printf("Ulaz %d, prva pojava %d    (%ld)\n", u->identifikator, u->stanje, u->trenutakZadnjePromjeneStanja);
    u->brojPromjenaStanja++;
    t.tv_sec = 0;
    t.tv_nsec = 1000000;                //postavljanje vremena za spavanje na 1 ms (bit će potrebno kod čekanja odgovora upravljača)
    while (simulationRunning)
    {
        u->stanje = rand() % 900 + 100;                 //upis promjene stanja
        u->trenutakZadnjePromjeneStanja = clock();
        printf("Ulaz %d, promjena stanja %d     (%ld)\n", u->identifikator, u->stanje, u->trenutakZadnjePromjeneStanja);
        u->brojPromjenaStanja++;
        while(u->zadnjiOdgovor != u->stanje)        //čekanje odgovora upravljačke dretve, provjera svake milisekunde
        {
            nanosleep(&t, &t2);
            if(!simulationRunning)                  //provjera trajanja simulacije (zaglavila bi se dretva ovdje u slučaju da je upravljač prekinut prije postavljanja odgovora)
                break;
        }
        int trajanje = (u->trenutakZadnjegOdgovora - u->trenutakZadnjePromjeneStanja);
        if((trajanje * 1000 /CLOCKS_PER_SEC) > u->perioda * 1000)
            u->brojPrekasnihOdgovora++;                     //ako je upravljaču bilo potrebno više od periode promjene stanja, odgovor je klasificiran kao zakašnjen
        u->sumaKasnjenjaOdgovora += trajanje;               //potrebno za izračun prosjeka vremena odgovora
        if(trajanje > u->maxVrijemeOdgovora)                
            u->maxVrijemeOdgovora = trajanje;
        
        int dodatnaOdgoda = (random() % u->K) * u->perioda;     //ulaz ne mora promijeniti stanje na svaki period, uvodimo odgodu (svaki put drugačija)
        while(clock() < (u->trenutakZadnjePromjeneStanja + (u->perioda + dodatnaOdgoda)*CLOCKS_PER_SEC))
            if(!simulationRunning)
                break;
    }

    if(u->identifikator < 100)                              //dodavanje kašnjenja kako bi ispis statistike bio strukturiran
    {
        t.tv_nsec = 10000000 * u->identifikator;
        t.tv_sec = 4;
    }
    else
    {
        t.tv_nsec = 10000000 * (u->identifikator % 10);
        t.tv_sec = 5;
    }
    nanosleep(&t, &t2);

    /*  Ispis statistike pojedine dretve    */
    printf("Statistika ulaza %d:\n", u->identifikator);
    printf("\t-broj promjena stanja: %d\n", u->brojPromjenaStanja);
    printf("\t-suma vremena odgovora: %ld ms\n", u->sumaKasnjenjaOdgovora*1000/CLOCKS_PER_SEC);
    printf("\t-prosjecno vrijeme odgovora: %ld ms\n", (u->sumaKasnjenjaOdgovora/u->brojPromjenaStanja)*1000/CLOCKS_PER_SEC);
    printf("\t-maksimalno vrijeme odgovora: %ld ms\n", u->maxVrijemeOdgovora*1000/CLOCKS_PER_SEC);
    printf("\t-broj problema: %d\n", u->brojPrekasnihOdgovora);
    printf("\t-udio problema: %f\n", (double)(u->brojPrekasnihOdgovora * 10000/u->brojPromjenaStanja)/10000);
}

/*  Funkcija koju aktivira CTRL+C. Označava kraj simulacije.    */
void kraj(int sig)
{
    simulationRunning = false;
    printf("Simulacija je gotova.\n");
    sleep(1);
}

/*  Main funkcija. Inicijalizira jednu upravljačku te varijabilan broj ulaznih dretvi. Broj dretvi
    koje želimo u sustavu predajemo preko argumenta prilikom pokretanja programa.
*/
int main(int argc, char *argv[])
{
    /*  postavljanje pozivanja funkcije kraj na SIGINT (CTRL+C) */
    struct sigaction act;
    act.sa_handler = kraj;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGTERM);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    BROJ_ULAZA = atoi(argv[1]);                         
    pthread_t thr_id[BROJ_ULAZA + 1];                   
    struct opisUlaza ulazi[BROJ_ULAZA];
    for(int i = 0; i < BROJ_ULAZA; i++){                //inicijalizacija parametara ulaznih dretvi 
        ulazi[i].brojPrekasnihOdgovora = 0;
        ulazi[i].brojPromjenaStanja = 0;
        ulazi[i].maxVrijemeOdgovora = 0;
        ulazi[i].stanje = 0;
        ulazi[i].sumaKasnjenjaOdgovora = 0;
        ulazi[i].zadnjiOdgovor = 0;
        ulazi[i].trenutakZadnjegOdgovora = 0;
        ulazi[i].trenutakZadnjePromjeneStanja = 0;
    }

    srand((unsigned)time(NULL));
    
    for (int i = 0; i < BROJ_ULAZA + 1; i++)
    {
        if (i == 0)
        {
            /*  Kreiranje upravljačke dretve    */
            if (pthread_create(&thr_id[i], NULL, (void *)upravljac, &ulazi) != 0)
            {
                printf("Greska pri stvaranju upravljačke dretve!\n");
                exit(1);
            }
            sleep(1);
        }
        else
        {
            /*  Kreiranje dretvi koje simuliraju pojedine ulaze sustava    */
            ulazi[i - 1].identifikator = i;
            ulazi[i - 1].K = 1;
            ulazi[i - 1].perioda = 2;
            ulazi[i - 1].prvaPojava = 3;
            if (pthread_create(&thr_id[i], NULL, (void *)ulaz, &ulazi[i - 1]) != 0)
            {
                printf("Greska pri stvaranju upravljačke dretve!\n");
                exit(1);
            }
        }
    }

    for (int i = 0; i < BROJ_ULAZA + 1; i++)        //ovdje main funkcija čeka da završe sve kreirane dretve
    {
        pthread_join(thr_id[i], NULL);
    }

    /*  Izračun i ispis statistike za sve ulazne dretve    */
    int promjeneStanja = 0, sumaVremena = 0, maxVrijeme = 0, brojProblema = 0;
    for(int i = 0; i < BROJ_ULAZA; i++)
    {
        promjeneStanja += ulazi[i].brojPromjenaStanja;
        sumaVremena += ulazi[i].sumaKasnjenjaOdgovora;
        brojProblema += ulazi[i].brojPrekasnihOdgovora;
        if(ulazi[i].maxVrijemeOdgovora > maxVrijeme)
            maxVrijeme = ulazi[i].maxVrijemeOdgovora;
    }

    printf("Statistika simulacije:\n");
    printf("\t-broj promjena stanja: %d\n", promjeneStanja);
    printf("\t-suma vremena odgovora: %ld ms\n", sumaVremena*1000/CLOCKS_PER_SEC);
    printf("\t-prosjecno vrijeme odgovora: %ld ms\n", (sumaVremena/promjeneStanja)*1000/CLOCKS_PER_SEC);
    printf("\t-maksimalno vrijeme odgovora: %ld ms\n", maxVrijeme*1000/CLOCKS_PER_SEC);
    printf("\t-broj problema: %d\n", brojProblema);
    printf("\t-udio problema: %f\n", (double)(brojProblema * 10000/promjeneStanja)/10000);
    
    return 0;
}