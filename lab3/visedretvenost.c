#include "stdio.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "sys/wait.h"
#include "unistd.h"
#include "semaphore.h"
#include <sched.h>

#define LAB3RT  (1)

static int BROJ = 100;

void trosi10ms(void)
{
    for(int i = 0; i < BROJ; i++)
    {
        asm volatile("" ::: "memory");
    }
}

void odrediBrojIteracija(void)
{
    int broj = 1000000;
    int vrti = 1;
    clock_t t0, t1;
    while(vrti == 1)
    {
        t0 = clock();
        trosi10ms();
        t1 = clock();
        if((t1 - t0)*1000/CLOCKS_PER_SEC >= 10)
        {
            vrti = 0;
        } 
        else 
        {
            BROJ = BROJ * 10;
        }
    }
    BROJ = BROJ * 10 / (t1-t0);
}

void simulirajMS(int ms)
{
    for(int i = 0; i < ms/10; i++)
    {
        trosi10ms();
    }
}

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
    sem_t sem;
};

/*  zastavica koja označava aktivnost simulacije    */
static bool simulationRunning = true;

/*  broj ulaza (dretvi) kojima je potrebno upravljati   */
static int BROJ_ULAZA = 0;

void *upravljac(struct opisUlaza *u)
{
    printf("Pokrenut upravljač %d!\n", u->identifikator);
    int zadnjeStanje = 0;   //polje kojim upravljačka dretva pamti prošla stanja ulaza
                 
    while (simulationRunning)
    {
        int stanje = u->stanje;
        if (stanje != zadnjeStanje)                  //ako je došlo do promjene stanja ulazne dretve, obradi ulaz, postavi odgovor(identičan trenutnom stanju)                                  
        {                                               //te upiši vrijeme postavljanja odgovora
            zadnjeStanje = stanje;
            printf("Upravljac -> obrada ulaza %d    (%ld)\n", u->identifikator, clock());
            int perioda = u->perioda * 1000;
            int num = rand() % 100 + 1;
            if (num <= 50)
                simulirajMS(0.1*perioda);
            else if (num > 50 && num <= 80)
                simulirajMS(0.2*perioda);
            else if (num > 80 && num < 95)
                simulirajMS(0.4*perioda);
            else
                simulirajMS(0.7*perioda);
            u->zadnjiOdgovor = stanje;
            u->trenutakZadnjegOdgovora = clock();
        }
        
    }
    printf("Završio upravljač %d!\n", u->identifikator);
    sem_post(&u->sem);
}

pthread_mutex_t lock;
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
        t.tv_nsec = 1000000; 
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
        t.tv_nsec = 10000000; 
        while(clock() < (u->trenutakZadnjePromjeneStanja + (u->perioda + dodatnaOdgoda)*CLOCKS_PER_SEC/10))
            nanosleep(&t, &t2);
        if(!simulationRunning)
            break;
    }
    sem_wait(&u->sem);
    pthread_mutex_lock(&lock);
    /*  Ispis statistike pojedine dretve    */
    printf("Statistika ulaza %d:\n", u->identifikator);
    printf("\t-broj promjena stanja: %d\n", u->brojPromjenaStanja);
    printf("\t-suma vremena odgovora: %ld ms\n", u->sumaKasnjenjaOdgovora*1000/CLOCKS_PER_SEC);
    printf("\t-prosjecno vrijeme odgovora: %ld ms\n", (u->sumaKasnjenjaOdgovora/u->brojPromjenaStanja)*1000/CLOCKS_PER_SEC);
    printf("\t-maksimalno vrijeme odgovora: %ld ms\n", u->maxVrijemeOdgovora*1000/CLOCKS_PER_SEC);
    printf("\t-broj problema: %d\n", u->brojPrekasnihOdgovora);
    printf("\t-udio problema: %f\n", (double)(u->brojPrekasnihOdgovora * 10000/u->brojPromjenaStanja)/10000);
    pthread_mutex_unlock(&lock);
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
    #if LAB3RT
        pthread_attr_t attr;
        struct sched_param prioritet;
        long nacin_rasporedivanja = SCHED_FIFO;

        //pocetna dretva
        prioritet.sched_priority = sched_get_priority_max(SCHED_FIFO) / 2;
        if (pthread_setschedparam(pthread_self(),nacin_rasporedivanja, &prioritet))
        {
            perror("Greska: pthread_setschedparam (dozvole?)");
            exit (1);
        }
        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, nacin_rasporedivanja);
    #endif
    odrediBrojIteracija();
    /*  postavljanje pozivanja funkcije kraj na SIGINT (CTRL+C) */
    struct sigaction act;
    act.sa_handler = kraj;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGTERM);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    BROJ_ULAZA = atoi(argv[1]);                         
    pthread_t thr_id[BROJ_ULAZA * 2];                   
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
    
    for (int i = 0; i < BROJ_ULAZA * 2; i++)
    {
        if (i >= BROJ_ULAZA)
        {
            /*  Kreiranje upravljačkih dretvi    */
            #if LAB3RT
            prioritet.sched_priority = sched_get_priority_max(SCHED_FIFO)/2 - 1 - i;
            pthread_attr_setschedparam(&attr, &prioritet);
            if (pthread_create(&thr_id[i], &attr, (void *)upravljac, &ulazi[i-BROJ_ULAZA]) != 0)
            {
                perror("Greska: pthread_create");
                exit (1);
            }
            #else
            if (pthread_create(&thr_id[i], NULL, (void *)upravljac, &ulazi[i-BROJ_ULAZA]) != 0)
            {
                printf("Greska pri stvaranju upravljačke dretve!\n");
                exit(1);
            }
            #endif
            sleep(1);
        }
        else
        {
            #if LAB3RT
            prioritet.sched_priority = sched_get_priority_max(SCHED_FIFO)/2 - 1;
            pthread_attr_setschedparam(&attr, &prioritet);
            #endif
            /*  Kreiranje dretvi koje simuliraju pojedine ulaze sustava    */
            ulazi[i].identifikator = i + 1;
            ulazi[i].K = 1;
            ulazi[i].perioda = i + 1;
            ulazi[i].prvaPojava = 3;
            #if LAB3RT
            if (pthread_create(&thr_id[i], &attr, (void *)ulaz, &ulazi[i]) != 0)
            {
                perror("Greska: pthread_create");
                exit (1);
            }
            #else
            if (pthread_create(&thr_id[i], NULL, (void *)ulaz, &ulazi[i]) != 0)
            {
                printf("Greska pri stvaranju ulazne dretve!\n");
                exit(1);
            }
            #endif
            sleep(1);
        }
    }

    for (int i = 0; i < BROJ_ULAZA * 2; i++)        //ovdje main funkcija čeka da završe sve kreirane dretve
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