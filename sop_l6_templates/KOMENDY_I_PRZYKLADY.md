# Komendy i przyklady uruchamiania

Ten plik tlumaczy najwazniejsze komendy i ich znaczenie dla szablonow:
- `template.c` (wersja "pelna")
- `template_lab.c` (wersja uproszczona pod laboratoria)

## 1) Kompilacja

### Komenda

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 template.c -o template -pthread
```

### Co robi

- `gcc` - kompilator C.
- `-std=c11` - standard jezyka C11.
- `-Wall -Wextra -Wpedantic` - wlacza duzo ostrzezen.
- `-O2` - umiarkowana optymalizacja kodu.
- `template.c` - plik wejsciowy.
- `-o template` - nazwa programu wynikowego.
- `-pthread` - wsparcie dla watkow, mutexow i semaforow POSIX.

Analogicznie dla wersji lab:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 template_lab.c -o template_lab -pthread
```

## 2) Uruchamianie programu

### Skladnia

```bash
./template <liczba_dzieci> <sciezka_do_pliku.txt>
```

lub:

```bash
./template_lab <liczba_dzieci> <sciezka_do_pliku.txt>
```

Przyklad:

```bash
./template 4 sample.txt
./template_lab 6 sample.txt
```

## 3) Przygotowanie testowego pliku .txt

Mozesz szybko utworzyc plik testowy:

```bash
printf 'Linia 1: 12345\nLinia 2: abcdef\nLinia 3: 67890\n' > sample.txt
```

## 4) Co zobaczysz w output

Przykladowe linie:

```text
[PARENT] child pid=1234 exit=0
[PARENT] child pid=1235 exit=126
```

- `exit=0` - dziecko zakonczylo sie normalnie.
- inny `exit` moze oznaczac celowa losowa "awarie" dziecka (pokazanie robust mutex).

Dalej zobaczysz np.:

```text
unnamed mmap: n o q n
named shm+mmap: N O Q N
alive_count (chronione robust mutex): 0
alive_count (chronione semaforem):    0
```

- `unnamed mmap` - wynik zapisany w pamieci dzielonej nienazwanej.
- `named shm+mmap` - wynik zapisany w pamieci dzielonej nazwanej.
- liczniki pokazuja ilu potomkow "zostalo" wg danej metody ochrony.

## 5) Najwazniejsze API uzyte w kodzie

- `fork()` - tworzenie procesow dzieci.
- `mmap(... MAP_SHARED | MAP_ANONYMOUS ...)` - pamiec dzielona nienazwana.
- `shm_open()` + `ftruncate()` + `mmap()` - pamiec dzielona nazwana.
- `pread()` - czytanie fragmentu pliku przez dziecko (offset + dlugosc).
- `pthread_mutex_t` z:
  - `PTHREAD_PROCESS_SHARED`
  - `PTHREAD_MUTEX_ROBUST`
  - obsluga `EOWNERDEAD` + `pthread_mutex_consistent()`
- `sem_init()/sem_wait()/sem_post()` - druga metoda ochrony wspolnego licznika.

## 6) Czeste problemy

- `Permission denied` przy `shm_open`:
  - zwykle problem srodowiska uruchomieniowego (sandbox / ograniczenia systemu).
- "stare" obiekty shm:
  - kod robi `shm_unlink`, ale po brutalnym ubiciu procesu moze zostac smiec.
  - nazwy shm sa stale, wiec kolejne uruchomienie moze wymagac cleanupu.

## 7) Szybki zestaw komend (kopiuj-wklej)

```bash
printf 'Test 111\nTest 222\nTest 333\n' > sample.txt
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 template.c -o template -pthread
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 template_lab.c -o template_lab -pthread
./template 4 sample.txt
./template_lab 4 sample.txt
```
