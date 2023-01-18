#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "sys/wait.h"
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "semaphore.h"
#include <time.h>

#define BROJ_ZADATAKA_A  1
#define BROJ_ZADATAKA_B  1
#define BROJ_ZADATAKA_C  1

#define PERIODA_ZADATKA_A  100      //u milisekundama
#define PERIODA_ZADATKA_B  500
#define PERIODA_ZADATKA_C  1000

static int red[] = {0, 1, -1, 0, 1, -1, 0, 1, 2, -1}; //za svaki prekid svakih 100 ms
static int t = 0;
static int ind[] = {0, 0, 0};
static int MAXTIP[] = {BROJ_ZADATAKA_A, BROJ_ZADATAKA_B, BROJ_ZADATAKA_C};

int zadatak_u_obradi;  //broj zadatka u obradi
int posao_u_obradi;     //pdbroj, moguće da zadatak u obradi opet izzazove prekid
int perioda;            //perioda izvođenja zadatka u obradi
int bez_prekoracenja;   //broj uzastopnih perioda bez prekoračenja

int druga_perioda;  //broj produženja obrade zadataka
int prekinuti;      //broj prekida zadataka u obradi

static bool simulationRunning = true;
int BROJ_ULAZA;

int daj_iduci(void)     //statički odlučuje koji je sljedeći ulaz za provjeru
{
    int sljedeci_zadatak = 0;
    int tip = red[t];
    t = (t+1) % 10;
    if(tip != -1)
    {
        if(tip == 0)
        {
            sljedeci_zadatak = ind[tip] + 1;
        }
        else if(tip == 1)
        {
            sljedeci_zadatak = ind[tip] + 1 + BROJ_ZADATAKA_A;
        }
        else if(tip == 2)
        {
            sljedeci_zadatak = ind[tip] + 1 + BROJ_ZADATAKA_A + BROJ_ZADATAKA_B;
        }
        ind[tip] = (ind[tip] + 1) % MAXTIP[tip];
        return sljedeci_zadatak - 1;
    }
    else
    {
        return -1;
    }
}

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
    sem_t sem;
};

int odredi_vrijeme_obrade(void)
{
    /*  Simulacija trajanja obrade pojedinog ulaza.
    Trajanje obrade:
        - 30 ms (20% slučajeva)
        - 50 ms (50% slučajeva)
        - 80 ms (20% slučajeva)
        - 120 ms (10% slučajeva)
    */
    int num = rand() % 100 + 1;
    if (num <= 20)
        return 30;
    else if (num > 20 && num <= 70)
        return 50;
    else if (num > 70 && num < 90)
        return 80;
    else
        return 120;
}

void obradi_zadatak(struct opisUlaza *u)
{
    obradi_zadatak:
    printf("Obradi zadatak %d\t(%ld)\n",zadatak_u_obradi, clock()*1000/CLOCKS_PER_SEC);
    int stanja[BROJ_ULAZA];
    for(int i = 0; i < BROJ_ULAZA; i++)
    {
        stanja[i] = (u+i)->stanje;
    }
    if(zadatak_u_obradi != -1)
    {
        if(perioda == 1 && bez_prekoracenja >= 10)
        {
            druga_perioda++;
            printf("Dozvoljavam drugu periodu zadatku %d\t(%ld)\n", zadatak_u_obradi, clock()*1000/CLOCKS_PER_SEC);
            perioda = 2;
            bez_prekoracenja = 0;
            goto nastavi_zadatak;
        }
        else
        {
            prekinuti++;
            printf("Prekidam obradu zadatka %d\t(%ld)\n", zadatak_u_obradi, clock()*1000/CLOCKS_PER_SEC);
        }
    }
    int uzmi_iduci = 1;
    int za_odraditi = 0;
    int moj_posao;

    while(uzmi_iduci == 1)
    {
        uzmi_iduci = 0;
        zadatak_u_obradi = daj_iduci();
        if(zadatak_u_obradi == -1 || (u+zadatak_u_obradi)->stanje == (u+zadatak_u_obradi)->zadnjiOdgovor)
        {
			posao_u_obradi = 0;
            return;
        }
        posao_u_obradi = clock()*1000/CLOCKS_PER_SEC;
        moj_posao = posao_u_obradi;
		perioda = 1;

        printf("Započinjem obradu zadatka %d\t(%ld)\n", zadatak_u_obradi, clock()*1000/CLOCKS_PER_SEC);
        (u+zadatak_u_obradi)->trenutakZadnjegOdgovora = clock();

        za_odraditi = odredi_vrijeme_obrade();
        struct timespec t, t2;
        t.tv_sec = 0;
        t.tv_nsec = 5000000;
        //TODO omogući prekidanje

        while(moj_posao == posao_u_obradi && za_odraditi > 0)
        {
            for(int i = 0; i < BROJ_ULAZA; i++)
            {
                if(stanja[i] != (u+i)->stanje)
                {
                    //printf("Prekid->zadatak%d\t(%ld)\n", i, clock()*1000/CLOCKS_PER_SEC);
                    goto obradi_zadatak;
                }
            }
            nastavi_zadatak:
            nanosleep(&t,NULL);
            za_odraditi -= 5;
            //printf("ZAodraditi%d\t(%ld)\n",za_odraditi, clock()*1000/CLOCKS_PER_SEC);
        }
        //TODO zabrani prekidanje

        if(moj_posao == posao_u_obradi && za_odraditi <= 0)
        {
            (u+zadatak_u_obradi)->zadnjiOdgovor = (u+zadatak_u_obradi)->stanje;
			(u+zadatak_u_obradi)->trenutakZadnjegOdgovora  = clock();
            printf("Gotova obrada zadatka %d\t(%ld)\n", zadatak_u_obradi, clock()*1000/CLOCKS_PER_SEC);
            zadatak_u_obradi = -1;
			posao_u_obradi = 0;
            if(perioda == 1)
            {
                bez_prekoracenja++;
            }
            else
            {
                uzmi_iduci = 1;
            }
        }
    }
}

//pthread_mutex_t lock;
/*  Dretva koja simulira pojedini ulaz. U sustavu ih je pokrenuto točno BROJ_ULAZA. */
void *ulaz(struct opisUlaza *u)
{    
    struct timespec t;
    t.tv_sec = u->prvaPojava;
    t.tv_nsec = 0;
    printf("Pokrenut ulaz %d, čekam prvu pojavu promjene stanja     (%ld)\n", u->identifikator, clock()*1000/CLOCKS_PER_SEC);
    nanosleep(&t,NULL);                 //spavanje dok ne prođe vrijeme postavljeno u strukturi opisa ulaza (vrijeme prvog postavljanja ulaza)

    u->stanje = rand() % 900 + 100;             //upis prve promjene stanja
    u->trenutakZadnjePromjeneStanja = clock();  //upis vremena prve promjene stanja
    printf("Ulaz %d, prva pojava %d    (%ld)\n", u->identifikator, u->stanje, u->trenutakZadnjePromjeneStanja*1000/CLOCKS_PER_SEC);
    u->brojPromjenaStanja++;
    t.tv_sec = 0;
    t.tv_nsec = 1000000;                //postavljanje vremena za spavanje na 1 ms (bit će potrebno kod čekanja odgovora upravljača)
    while (simulationRunning)
    {
        u->stanje = rand() % 900 + 100;                 //upis promjene stanja
        u->trenutakZadnjePromjeneStanja = clock();
        printf("Ulaz %d, promjena stanja %d     (%ld)\n", u->identifikator, u->stanje, u->trenutakZadnjePromjeneStanja*1000/CLOCKS_PER_SEC);
        u->brojPromjenaStanja++;
        t.tv_nsec = 1000000; 
        while(u->zadnjiOdgovor != u->stanje)        //čekanje odgovora upravljačke dretve, provjera svake milisekunde
        {
            nanosleep(&t, NULL);
            if(!simulationRunning)                  //provjera trajanja simulacije (zaglavila bi se dretva ovdje u slučaju da je upravljač prekinut prije postavljanja odgovora)
                break;
        }
        int trajanje = (u->trenutakZadnjegOdgovora - u->trenutakZadnjePromjeneStanja);
        if((trajanje * 1000 /CLOCKS_PER_SEC) > u->perioda)
            u->brojPrekasnihOdgovora++;                     //ako je upravljaču bilo potrebno više od periode promjene stanja, odgovor je klasificiran kao zakašnjen
        u->sumaKasnjenjaOdgovora += trajanje;               //potrebno za izračun prosjeka vremena odgovora
        if(trajanje > u->maxVrijemeOdgovora)                
            u->maxVrijemeOdgovora = trajanje;
        
        int dodatnaOdgoda = (random() % u->K) * u->perioda;     //ulaz ne mora promijeniti stanje na svaki period, uvodimo odgodu (svaki put drugačija)
        t.tv_nsec = 20000000; 
        while(clock() < (u->trenutakZadnjePromjeneStanja + (u->perioda + dodatnaOdgoda)*CLOCKS_PER_SEC/1000));
            nanosleep(&t, NULL);
        if(!simulationRunning)
            break;
    }
    /*
    sem_wait(&u->sem);
    pthread_mutex_lock(&lock);
      Ispis statistike pojedine dretve    
    printf("Statistika ulaza %d:\n", u->identifikator);
    printf("\t-broj promjena stanja: %d\n", u->brojPromjenaStanja);
    printf("\t-suma vremena odgovora: %ld ms\n", u->sumaKasnjenjaOdgovora*1000/CLOCKS_PER_SEC);
    printf("\t-prosjecno vrijeme odgovora: %ld ms\n", (u->sumaKasnjenjaOdgovora/u->brojPromjenaStanja)*1000/CLOCKS_PER_SEC);
    printf("\t-maksimalno vrijeme odgovora: %ld ms\n", u->maxVrijemeOdgovora*1000/CLOCKS_PER_SEC);
    printf("\t-broj problema: %d\n", u->brojPrekasnihOdgovora);
    printf("\t-udio problema: %f\n", (double)(u->brojPrekasnihOdgovora * 10000/u->brojPromjenaStanja)/10000);
    pthread_mutex_unlock(&lock);
    */
}

/*  Funkcija koju aktivira CTRL+C. Označava kraj simulacije.    */
void kraj(int sig)
{
    simulationRunning = false;
    printf("Simulacija je gotova.\n");
    sleep(1);
}


int main(void)
{
    zadatak_u_obradi = -1;  //broj zadatka u obradi
    posao_u_obradi = 0;
    perioda = 0;            //perioda izvođenja zadatka u obradi
    bez_prekoracenja = 10;   //broj uzastopnih perioda bez prekoračenja
    druga_perioda = 0;  //broj produženja obrade zadataka
    prekinuti = 0; 
    
     /*  postavljanje pozivanja funkcije kraj na SIGINT (CTRL+C) */
    struct sigaction act;
    act.sa_handler = kraj;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGTERM);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
    srand((unsigned)time(NULL));

    BROJ_ULAZA = BROJ_ZADATAKA_A + BROJ_ZADATAKA_B + BROJ_ZADATAKA_C;                         
    pthread_t thr_id[BROJ_ULAZA];                   
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
        sem_init(&ulazi[i].sem,0,0);
    }

    srand((unsigned)time(NULL));
    
    for (int i = 0; i < BROJ_ULAZA; i++)
    {
        {
            /*  Kreiranje dretvi koje simuliraju pojedine ulaze sustava    */
            ulazi[i].identifikator = i;
            ulazi[i].K = 1;
            if(i < BROJ_ZADATAKA_A)
            {
                ulazi[i].perioda = PERIODA_ZADATKA_A;
            }
            else if(i >= BROJ_ZADATAKA_A && i < (BROJ_ZADATAKA_A + BROJ_ZADATAKA_B))
            {
                ulazi[i].perioda = PERIODA_ZADATKA_B;
            }
            else
            {
                ulazi[i].perioda = PERIODA_ZADATKA_C;
            }
            ulazi[i].prvaPojava = 0;
            if (pthread_create(&thr_id[i], NULL, (void *)ulaz, &ulazi[i]) != 0)
            {
                printf("Greska pri stvaranju upravljačke dretve!\n");
                exit(1);
            }
        }
    }
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = 20000000;
    nanosleep(&t,NULL);

    while(simulationRunning)
    {
        clock_t t = clock();
        obradi_zadatak(ulazi);
        while(clock() < t + 0.1*CLOCKS_PER_SEC);
    }

    for (int i = 0; i < BROJ_ULAZA; i++)        //ovdje main funkcija čeka da završe sve kreirane dretve
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
    printf("\t-broj dozvoljenih drugih perioda: %d\n", druga_perioda);
    printf("\t-broj prekinutih zadataka: %d\n", prekinuti);
    
    return 0;
}

