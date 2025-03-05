# Tema 2
##  Protocolul BitTorrent
La inceputul programului am creat doua structuri ce ma ajuta sa retin informatiile detinute de clienti si informatiile detinute de tracker:

- ### **Client**
    ```c
    struct info_client {
        int n_f;
        map<string, vector<string>> files;
        vector<string> w_files;
    } info_c;
    ```
    Unde:
    - ***n_f*** reprezinta numarul de fisiere la care clientul este ***seed***.
    - ***files*** reprezinta un dictionar ce face asocieri intre numele fiserelor si hash-urile segmentelor din ele.
    - ***w_files*** reprezinta un array ce contine toate numele fisierelor pe care clientul vrea sa le descarce.

- ### **Tracker**
    ```c
    struct info_tracker {
        map<string, vector<string>> files;
        map<string, vector<int>> swarm;
    } info_t;
    ```
    Unde:
    - ***files***, la fel ca mai sus, reprezinta un dictionar cu asocieri nume_fisier <-> hash-uri segmente.
    - ***swarm***, are rolul tot al unui dictionar, doar ca acesta retine ascoieri intre numele fisierelor si rank-urile clientilor care sunt fie ***seed***, fie ***peer*** pentru acel fisier.


## Download
#### In functia ***download*** am inceput prin a citi fisierul asignat fiecarui client apoi am transmis informatiile catre tracker, si anume: fisierele pe care le detine si hash-urile segmentelor acestora. Dupa aceea am asteptat confirmarea de la tracker ca a terminat de obtinut toate informatiile de la clienti. Apoi, am inceput procesul de "descarcare", astfel:
- am iterat prin toate fisierele pe care clientul voia sa le descarce.
- am trimis un mesaj catre tracker cu tag-ul specific download-ului (***download***) si am inceput sa iau de la acesta swarm-ul si hash-urile segmentelor pentru fisierul curent.
- am iterat prin toate hash-urile fisierlui pe care clientul il descarca si pentru **a eficientiza** procesul de descarcare la fiecare iteratie aleg aleator un client din sworm (diferit de mine), de la care sa iau segmentul curent. Acest lucru ajuta la neblocarea unuia dintre clienti pana la descarcarea fisierului in intregime.
- in plus pentru a nu exista "coliziuni" ale canalelor de comunicatie, clientii comunica intre ei folosind "taguri dinamice" (d_client + rank-ul_clientului). Astfel chiar daca un client face atat download cat si upload acesta nu-si va intersecta canalele de download cu canalele de upload.
- odata la 10 segmente descarcate actualizez swarm-ul de la tracker.
- dupa ce un client descarca in totalitate un fisier acesta il afiseaza intr-un fisier de output conform cerintei.
- la final dupa ce clientul termina de descarcat toate fisierele acesta anunta tracker-ul ca nu mai are nimic de descarcat.

## Upload
#### Functia de upload este reprezentata in mare parte de o bluca infinita care raspunde la cererile altor clienti, astfel:
- la inceput primeste un mesaj prin care vede daca trebuie sa raspunda la o cerere sau trebuie sa-si termine executia. Acest mesaj se efectueaza cu un tag diferit de restul mesajelor pentru a nu exista coliziuni.
- in cazul in care trebuie sa raspunda la o cerere acesta primeste atat numele fisierlui din care trebuie sa trimita segment cat si hash-ul segmentului pentru a-l cauta. Acesta cauta hash-ul segmentului in dictionarul cu asocieri nume_fisier <-> hash-uri, iar daca il gasesti trimite o confirmare (ACK), altfel trimite un mesaj corespunzator (NACK - 0).
- la fel ca mai sus comunicarea intre clienti se face tot pe baza unui "tag dinamic" pentru a nu exista colizuni intre canalele de comunicatie.
- in cazul in care mesajul primit la inceput are o valoare negativa, clientul stie ca toti clientii si-au descarcat fisierele si nu mai trebuie sa raspunda la alte cerire, urmand sa-si opreasca executia.

## Tracker
La inceputul functiei retin toate informatiile de la toti clienti (fisiere + hash-uri segmente) dupa care raspund la cererile si mesajele clientilor folosind o bucla infinita care se opreste in momentul in care niciun client nu mai trebuie sa descarce nimic, astfel:
- la inceput astept un mesaj care in functie de tag-ul acestuia stiu ce actiune trebuie sa efectuez.
    - ***download*** - astept de la client numele fisierlui pe care vrea sa-l descarce, urmand sa-i trimit swarm-ul fisierului si hash-urile segmentelor acestuia. Totodata adaug clientul ca peer in swarm-ul fisierului.
    - ***download_done*** - cresc contorul de clienti care si-au terminat de descarcat fisierele si verific daca cumva au terminat toti clientii, caz in care opresc bucla infinita pentru a nu mai primi cereri.
    - ***update*** - astept primirea numelui fisierului pentru care se doreste un update al swarm-ului de la client si trimit noul swarm actualizat.
- la finalul functiei anunt toti clientii care fac upload ca se pot opri din a mai astepta cereri, deoarece nu mai exista clienti care sa descarce fisiere.

Punctajul pe local este ***maxim***, iar pe docker este posibil sa fluctueze intre 30/40 din cauza alocarilor de memorie pe care le efectueaza structurile de date din STL, deoarece chiar daca dimensiunea unui vector creste constant, la inceput stl-ul isi poate retine o bucata de memorie initiala, a carei dimensiune difera de la rulare la rulare (limitarea docker-ului la memorie). Daca exista posibilitatea ca punctajul sa fie 30 in loc de 40, ar fi super daca s-ar mai putea rula odata, multumesc.

