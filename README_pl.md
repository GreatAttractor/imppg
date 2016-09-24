# ImPPG (Image Post-Processor)
Copyright (C) 2015, 2016 Filip Szczerek (ga.software@yahoo.com)

wersja 0.5.1 (2016-10-02)

*Niniejszy program ABSOLUTNIE nie jest objęty JAKĄKOLWIEK GWARANCJĄ. Jest to wolne oprogramowanie na licencji GNU GPL w wersji 3 (lub dowolnej późniejszej) i można je swobodnie rozpowszechniać pod pewnymi warunkami: zob. pełny tekst licencji w pliku LICENSE.*

(kodowanie tego pliku: UTF-8)

----------------------------------------

- 1\. Wprowadzenie
- 2\. Interfejs użytkownika
- 3\. Obsługiwane formaty graficzne
- 4\. Przetwarzanie obrazów
  - 4\.1\. Normalizacja jasności
  - 4\.2\. Dekonwolucja Lucy-Richardson
  - 4\.3\. Unsharp masking
  - 4\.4\. Krzywa tonalna
- 5\. Zapisywanie/wczytywanie ustawień
- 6\. Przetwarzanie wsadowe (wielu plików)
- 7\. Wyrównywanie sekwencji obrazów
  - 7\.1\. Stabilizacja kontrastowych obszarów (korelacja fazowa)
  - 7\.2\. Stabilizacja krawędzi tarczy słonecznej
- 8\. Pozostałe
- 9\. Znane problemy
- 10\. Pobieranie
- 11\. Budowanie ze źródeł
  - 11\.1\. Budowanie w systemie Linux i podobnych z użyciem narzędzi GNU (lub kompatybilnych)
  - 11\.2\. Budowanie pod MS Windows
  - 11\.3\. Język UI
- 12\. Historia zmian


----------------------------------------
## 1. Wprowadzenie

ImPPG (*Image Post-Processor*) dokonuje typowych operacji obróbki obrazu, w poniższej kolejności:

  - normalizacja poziomu jasności
  - dekonwolucja Lucy-Richardson z ustalonym kernelem gaussowskim
  - wygładzanie lub wyostrzanie metodą unsharp masking
  - zmiana krzywej tonalnej

(wszystkie kroki są opcjonalne). Przetwarzanie odbywa się z użyciem arytmetyki zmiennoprzecinkowej pojedynczej precyzji (32 bity). Ustawienia wszystkich powyższych etapów można zapisać w pliku i użyć do przetwarzania wsadowego wielu obrazów.

ImPPG umożliwia również wyrównanie sekwencji obrazów, które mogą być znacznie przesunięte względem siebie w przypadkowy sposób. Funkcja ta może by przydatna np. do przygotowania animacji poklatkowej Słońca, gdzie kolejne klatki uległy przesunięciu wskutek niedokładnego prowadzenia montażu teleskopu. Inne możliwe zastosowania to wygładzanie animacji poklatkowych krajobrazu lub przygotowanie surowych klatek (z dużymi drganiami obrazu) do stackowania.

Wyrównywanie (z subpikselową dokładnością) odbywa się metodą korelacji fazowej (która automatycznie dopasowuje do siebie najbardziej kontrastowe obszary) lub poprzez wykrywanie i stabilizację krawędzi tarczy słonecznej.


----------------------------------------
## 2. Interfejs użytkownika

  - Panel elementów kontrolnych sterujących obróbką, początkowo zadokowany po lewej stronie okna głównego, może zostać odłączony lub zadokowany po prawej stronie.

  - Panel sterujący obróbką oraz okno edytora krzywej tonalnej mogą zostać zamknięte i przywrócone poleceniami z menu `Widok` lub przyciskami paska narzędziowego.
    
  - Zmiany ustawień dekonwolucji L-R, unsharp maskingu i krzywej tonalnej wpływają tylko na bieżące zaznaczenie (ograniczone wykropkowaną lub oznaczoną odwróconym poziomem jasności ramką). By zastosować je do całego obrazu, należy użyć funkcji `Zaznacz (i poddaj obróbce) wszystko` z menu `Edycja` lub odpowiedniego przycisku z paska narzędziowego.
    
  - Bieżące zaznaczenie można zmienić w każdej chwili przeciągając myszą z wciśniętym lewym przyciskiem. Zmiana zaznaczenia przerywa rozpoczęte już przetwarzanie (o ile było w toku).
    
  - Im mniejszy zaznaczony obszar, tym szybciej przebiega obróbka. Do precyzyjnej regulacji *sigmy* dekonwolucji L-R zaleca się zaznaczenie w miarę małego obszaru; dzięki temu efekty poruszania suwakiem *sigmy* będą widoczne prawie natychmiastowo. Odświeżanie jest nieco wolniejsze, gdy bieżące powiększenie jest różne od 100%.
    
  - Powiększenie widoku można zmieniać przyciskami paska narzędziowego, pozycjami w menu `Widok` bądź kombinacją `Ctrl+rolka myszy`.
    
  - Widok można przewijać przeciągając go z wciśniętym środkowym przyciskiem myszy.

  - Wielkość ikon narzędziowych można zmienić w menu `Ustawienia`.

    
----------------------------------------
## 3. Obsługiwane formaty graficzne

Akceptowane formaty wejściowe: BMP, JPEG, PNG, TIFF (większość głębi bitowych i metod kompresji), TGA i inne poprzez bibliotekę FreeImage, FITS. Obraz jest przetwarzany i zapisywany w odcieniach szarości w jednym z formatów: BMP 8-bitowy, PNG 8-bitowy, TIFF 8-bitowy, 16-bitowy, 32-bitowy zmiennoprzecinkowy (bez kompresji bądź z kompresją LZW lub ZIP), FITS 8-, 16-, 32-bitowy zmiennoprzecinkowy.

Obrazy wyjściowe po użyciu funkcji wyrównywania sekwencji zapisywane są w formacie TIFF (bez kompresji) z zachowaniem liczby kanałów i głębi bitowej (oprócz 8-bitowych z paletą; te zostaną skonwertowane na 24-bitowe RGB). Pliki wejściowe FITS zapisywane są jako FITS o takiej samej głębi bitowej.


----------------------------------------
## 4. Przetwarzanie obrazów

Ilustrowany samouczek dostępny jest pod adresem http://greatattractor.github.io/imppg/tutorial/tutorial_en.html

----------------------------------------
### 4.1. Normalizacja jasności

ImPPG jest w stanie sprowadzić poziom jasności obrazu wejściowego (przed wszystkimi pozostałymi etapami obróbki) do zadanego przez użytkownika zakresu od `min` do `max` (wartości w procentach maksymalnej jasności; 0% = czarny, 100% = biały). Najciemniejsze piksele wejściowe otrzymują wartość `min`, najjaśniejsze: wartość `max`.

Zarówno `min` jak i `max` mogą być mniejsze niż 0% i większe niż 100% (może to spowodować obcięcie histogramu). Dopuszczalne jest również, by `max` była mniejsza od `min`; wówczas poziomy jasności zostaną odwrócone (obraz stanie się negatywem; można to również osiągnąć edytując krzywą tonalną, zob. 4.4).

Normalizacja może być przydatna podczas obróbki sekwencji astronomicznych „stacków” zarejestrowanych w czasie zmiennej przejrzystości powietrza (zatem z różnymi poziomami jasności).

Dostęp:
    menu: `Ustawienia`/`Normalizacja poziomu jasności...`
    
    
----------------------------------------
### 4.2. Dekonwolucja Lucy-Richardson

ImPPG wyostrza obrazy stosując dekonwolucję Lucy-Richardson z ustalonym kernelem gaussowskim. Szerokość kernela dana jest *sigmą* funkcji Gaussa; zwiększanie jej sprawia, że ostrzenie jest bardziej gruboziarniste.

Zalecana liczba iteracji dekonwolucji: 30 do 70. Wartość 0 wyłącza dekonwolucję L-R.

Przełącznik `Ogranicz ringing` aktywuje eksperymentalną funkcję redukcji *ringingu* (halo) wokół prześwietlonych obszarów (np. tarczy słonecznej na obrazie protuberancji), powodowanego przez wyostrzanie.

Dostęp:
    zakładka `Dekonwolucja Lucy-Richardson` na panelu kontrolnym (po lewej stronie okna głównego)

    
----------------------------------------    
### 4.3. Unsharp masking

Unsharp masking służy do końcowego wyostrzania (niezależnie od dekonwolucji L-R) lub wygładzania obrazu. Parametr `sigma` określa szerokość kernela gaussowskiego; im większa wartość, tym bardziej gruboziarniste ostrzenie/wygładzanie. `Poziom` reguluje natężenie efektu. Wartość < 1 wygładza, 1 pozostawia obraz bez zmian, wartość > 1 wyostrza.

#### Tryb adaptatywny

W trybie tym `poziom` jest zmienny i zależy od jasności (nieprzetworzonego) obrazu wejściowego. Dla obszarów ciemniejszych niż `próg − szerokość granicy` przybiera on wartość `poziom min.`, dla jaśniejszych niż `próg + szerokość granicy`: `poziom max.`. Pomiędzy nimi `poziom` zmienia się płynnie od `poziom min.` do `poziom max.`.

Przykładowa sytuacja, gdzie tryb adaptatywny może być przydatny, to „wyciąganie” słabych protuberancji słonecznych poprzez manipulację krzywą tonalną. Zazwyczaj podkreśli to również szum w obrazie (zwłaszcza gdy użyto dekonwolucji L-R), jako że ciemniejsze obszary cechują się mniejszym stosunkiem sygnału do szumu. Ustawiając dla `poziom max.` wartość wyostrzającą (>1,0) odpowiednią dla wnętrza tarczy słonecznej, ale wartość 1,0 (tj. brak wyostrzania) dla `poziom min.` oraz `próg` wypadający w strefie przejściowej między tarczą a warstwą protuberancji, zapewnimy, że protuberancje i tło nie będą dodatkowo wystrzone. Można je wręcz wygładzić ustawiając `poziom min.` poniżej 1,0 (tarcza pozostanie wyostrzona).

Dopuszczalne jest również ustawienie `poziom min.` > `poziom max.`.

Dostęp:
    ramka `Unsharp masking` na panelu kontrolnym (po lewej stronie okna głównego)
    

----------------------------------------
### 4.4. Krzywa tonalna

Edytor krzywej tonalnej umożliwia zmianę odwzorowania wejściowych poziomów jasności w wyjściowe. Krzywa tonalna aplikowana jest dopiero po wszystkich pozostałych krokach obróbki. Histogram pokazany w tle okna edytora krzywej odpowiada wynikowi tychże kroków **przed** zastosowaniem krzywej.

Dostęp:
    menu: `Widok`/`Panele`/`Krzywa tonalna` lub odpowiedni przycisk paska narzędziowego

    
----------------------------------------
## 5. Zapisywanie/wczytywanie ustawień

Wszystkie ustawienia opisane w **4.1-4.4** można zapisać do/wczytać z pliku (w formacie XML). Plik z ustawieniami jest niezbędny, by móc użyć funkcji przetwarzania wsadowego.

Dostęp:
    menu: `Plik` lub odpowiednie przyciski paska narzędziowego

    
----------------------------------------
## 6. Przetwarzanie wsadowe (wielu plików)

Korzystając z pliku ustawień obróbki (zob. **5**) możliwe jest automatyczne zastosowanie ich do wielu obrazów. Obrazy wyjściowe zapisane zostaną w podanym folderze pod nazwami z sufiksem `_out`.

Dostęp:
    menu: `Plik`/`Przetwarzanie wsadowe...`


----------------------------------------    
## 7. Wyrównywanie sekwencji obrazów

Lista `Obrazy wejściowe` w oknie dialogowym `Wyrównywanie obrazów` wyświetla pliki wejściowe w kolejności, w jakiej ImPPG ma dokonać wyrównania sekwencji. Pliki można usuwać i przenosić pojedynczo korzystając z przycisków w prawym górnym rogu listy.

Efektem zaznaczenia pola `Wyrównywanie subpikselowe` jest płynniejsza animacja i mniejszy dryf obrazu. Zapisywanie plików wejściowych (ale nie samo wyrównywanie) będzie trwać nieco dłużej. Bardzo drobne, 1-pikselowe szczegóły (o ile są obecne) mogą ulec nieznacznemu rozmyciu.

Opcja `Obetnij do części wspólnej` obcina obrazy wyjściowe do ich największego wspólnego obszaru. Opcja `Rozszerz do prostokąta obejmującego` rozszerza je do najmniejszej prostokątnej otoczki.

Pliki wyjściowe zapisane zostaną w wybranym katalogu docelowym pod nazwami z sufiksem `_aligned`. Pliki FITS zostaną zapisane jako FITS, pozostałe formaty: jako TIFF.


----------------------------------------
### 7.1. Stabilizacja kontrastowych obszarów (korelacja fazowa)

Metoda uniwersalna. Dąży do zminimalizowania ruchu kontrastowych obszarów (np. plam słonecznych, filamentów, protuberancji, kraterów). W pewnych sytuacjach może to być niepożądane, np. w wielogodzinnej animacji poklatkowej plamy słonecznej w pobliżu krawędzi tarczy; korelacja fazowa będzie zwykle dążyć do utrzymania nieruchomo plamy, a nie krawędzi.


----------------------------------------
### 7.2. Stabilizacja krawędzi tarczy słonecznej

Wykrywa i dopasowuje pozycje krawędzi tarczy Słońca. Im większy łuk krawędzi jest widoczny na obrazach wejściowych, tym skuteczniejsze wyrównanie. Zaleca się, by wyrównywane obrazy były już wyostrzone. Wymagania:
  - tarcza musi być jaśniejsza niż tło
  - brak winietowania i uwydatnionego obróbką pociemnienia brzegowego
  - tarcza nie może być przesłonięta przez Księżyc

Operacje takie jak zmiana obrazu na pełny/częściowy negatyw czy użycie „przyciemniającej” krzywej tonalnej można przeprowadzić już po wyrównaniu.


Dostęp:
    menu: `Narzędzia`/`Wyrównaj sekwencję obrazów...`

    
----------------------------------------
## 8. Pozostałe

ImPPG przechowuje pewne ustawienia (np. pozycję i rozmiar okna głównego) w pliku INI, w miejscu zależnym od używanej platformy. W nowszych wersjach MS Windows jest to `%HOMEPATH%\AppData\Roaming\imppg.ini`, gdzie `%HOMEPATH%` zwykle oznacza `C:\Users\<użytkownik>`. W systemie Linux: `~/.imppg`.


----------------------------------------
## 9. Znane problemy

  - wxGTK 3.0.2, Fedora 20: Wszystkie przyciski narzędziowe reagują poprawnie, ale czasami wyświetlony stan (wciśnięty lub nie) jest niewłaściwy po użyciu opcji z menu `View` lub przycisków zamykania panelu kontrolnego bądź edytora krzywej tonalnej.
    
  - (wxGTK) W lutym 2015 niektóre motywy GTK nie funkcjonują poprawnie (np. „QtCurve”, ale już nie „Raleigh”). W ImPPG może się to objawiać następująco:
    - okno dialogowe służące do otwierania plików nie pokazuje zawartości po jego otwarciu; zawartość jest odświeżana dopiero po zmianie rozmiaru okna przez użytkownika
    - program zostaje zamknięty z błędem po próbie wyboru folderu wyjściowego w oknie dialogowym z parametrami przetwarzania wsadowego
    - program zawiesza się przy próbie zmiany typu pliku w oknie dialogowym `Otwórz plik graficzny`

Rozwiązanie: zmienić motyw GTK na „Raleigh” (np. w Fedorze użyć narzędzia „GTK+ Appearance”).
    
  - (wxGTK) Gdy używane są wxWidgets zbudowane z GTK 3, pozycja okna edytora krzywej tonalnej nie jest przywracana po uruchomieniu programu (z GTK 2 problem nie występuje).


----------------------------------------
## 10. Pobieranie

Kod źródłowy ImPPG i pliki wykonywalne dla MS Windows można pobrać pod adresem:  
    https://github.com/GreatAttractor/imppg/releases


----------------------------------------
## 11. Budowanie ze źródeł

Budowanie ze źródeł wymaga narzędzi do kompilacji C++ (zgodnych z C++11), CMake, bibliotek Boost w wersji 1.57.0 lub późniejszej (choć wcześniejsze też mogą działać) oraz wxWidgets 3.0. Do obsługi większej liczby formatów graficznych potrzebna jest biblioteka FreeImage w wersji co najmniej 3.14.0. Bez niej obsługiwane są jedynie: BMP 8-, 24- i 32-bitowe, TIFF mono lub RGB, 8 lub 16 bitów na kanał (bez kompresji). Obsługę plików FITS (opcjonalną) zapewnia biblioteka CFITSIO. Przetwarzanie wielowątkowe wymaga kompilatora obsługującego OpenMP (np. GCC 4.2 lub nowsze, MS Visual C++ 2008 lub nowsze (wersje płatne), MS Visual C++ 2012 Express lub nowsze).

Obsługę CFITSIO i FreeImage można wyłączyć edytując plik `config.in` (domyślnie są włączone).

By wyczyścić stworzoną przez CMake konfigurację budowania, należy usunąć `CMakeCache.txt` i katalog `CMakeFiles`.

Plik wykonywalny musi znajdować się w tym samym miejscu, co podkatalog `images` i podkatalogi z plikami językowymi.

*Symbol $ w poniższych przykładach oznacza znak zachęty (prompt) linii poleceń.*


### 11.1. Budowanie w systemie Linux i podobnych z użyciem narzędzi GNU (lub kompatybilnych)

*Uwaga: CMake wymaga dostępności narzędzia `wx-config`, by wykryć wxWidgets i skonfigurować związane z nimi ustawienia. Czasami nazwa może być inna, np. w Fedorze 23 z pakietami wxGTK3 z repozytorium jest to `wx-config-3.0`. Można temu zaradzić np. tworząc dowiązanie symboliczne:*
```
    $ sudo ln -s /usr/bin/wx-config-3.0 /usr/bin/wx-config  
```

Po upewnieniu się, że `wx-config` jest dostępne, wykonać:
```
    $ cmake -G "Unix Makefiles"
```
Zostanie utworzony natywny plik `Makefile`. Dopóki `config.in` nie zostanie zmieniony, nie trzeba więcej uruchamiać CMake.

By zbudować ImPPG, wykonać:
```
    $ make
```
W katalogu ze źródłami pojawi się plik wykonywalny `imppg`.


----------------------------------------
### 11.2. Budowanie pod MS Windows

Jako że w tym wypadku nie ma standardowej lokacji plików nagłówkowych i bibliotek, należy zacząć od otwarcia `config.in` i odpowiedniego ustawienia ścieżek do ww. plików.


#### Budowanie z użyciem MinGW (32- lub 64-bitowy)

Upewnić się, że narzędzia MinGW są na ścieżce wyszukiwania (np. wykonać `set PATH=%PATH%;c:\MinGW\bin`) i wykonać:
```
    $ cmake -G "MinGW Makefiles"
```
Zostanie utworzony natywny plik `Makefile`. Dopóki `config.in` nie zostanie zmieniony, nie trzeba więcej uruchamiać CMake.

By zbudować ImPPG, wykonać:
```
    $ mingw32-make
```
W katalogu ze źródłami pojawi się plik wykonywalny `imppg.exe`.


#### Budowanie z użyciem Microsoft C++ (z Windows SDK lub Visual Studio; 32- lub 64-bitowy)

Upewnić się, że środowisko jest skonfigurowane odpowiednio dla narzędzi MSVC (np. wykonać `C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\vcvars32.bat` dla konfiguracji 32-bitowej lub `C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat` dla 64-bitowej), następnie wykonać:
```
    $ cmake -G "NMake Makefiles"
```
Zostanie utworzony natywny plik `Makefile`. Dopóki `config.in` nie zostanie zmieniony, nie trzeba więcej uruchamiać CMake.

By zbudować ImPPG, wykonać:
```
    $ nmake
```
W katalogu ze źródłami pojawi się plik wykonywalny `imppg.exe`.

CMake jest w stanie również stworzyć pliki projektu dla MS Visual Studio (np. `cmake -G "Visual Studio 12 2013"` utworzy je dla VS 2013). Uruchomienie `cmake -G` wyświetli listę obsługiwanych generatorów.


----------------------------------------
### 11.3. Język UI

ImPPG obsługuje wielojęzyczny interfejs użytkownika poprzez wbudowane funkcje regionalizacji biblioteki wxWidgets. Wszystkie wymagające tłumaczenia napisy w kodzie źródłowym otoczone są makrem `_()`. Dla dodania nowego tłumaczenia wymagany jest pakiet `GNU gettext` i wykonanie następujących kroków:

- ekstrakcja napisów do przetłumaczenia z kodu źródłowego do pliku PO poprzez wykonanie:
```
  $ xgettext -k_ *.cpp *.h -o imppg.po
```

- przetłumaczenie napisów UI, tj. edycja wpisów `msgstr` w `imppg.po`

- konwersja `imppg.po` do postaci binarnej:
```
  $ msgfmt imppg.po -o imppg.mo
```

- umieszczenie `imppg.mo` w podkatalogu o nazwie będącej kodem języka (np. `pl`, `fr-ca`)

- dopisanie języka do tablic w `c_MainWindow::SelectLanguage()` (`main_window.cpp`)

Dystrybucja binarna ImPPG potrzebuje jedynie plików MO (binarnych). Oprócz pliku(ów) `imppg.mo` potrzebny jest również plik językowy wxWidgets (z napisami takimi jak standardowe pozycje menu, np. „Otwórz”, etykiety elementów kontrolnych, np. „Przeglądaj” itd.). Pliki te znajdują się w katalogu `<źródła_wxWidgets>/locale`. Pod Windows plik `<język>.mo` z wxWidgets musi być dostępny jako `<katalog_imppg>/<język>/wxstd3.mo`. W przypadku systemów operacyjnych, które przechowują pliki językowe MO w jednym miejscu (np. Linux), wystarczy posiadać instalację wxWidgets.


----------------------------------------
## 12. Historia zmian
```
0.5.1 (2016-10-02)
    Nowe funkcje:
    – Lista ostatnio używanych ustawień

    Ulepszenia:
    – Ikony narzędziowe o wysokiej rozdzielczości
    – Usprawnienie rysowania krzywej tonalnej na ekranach o wysokiej rozdzielczości

0.5 (2016-01-02)
    Nowe funkcje:
    – Adaptatywny unsharp masking
    
    Ulepszenia:
    – Suwaki numeryczne można przewijać klawiszami kursora
    – Szerokość panelu kontrolnego jest zapamiętywana
    – Użycie CMake do budowania ze źródeł

0.4.1 (2015-08-30)
    Ulepszenia:
    – Suwaki numeryczne można zmieniać z 1-pikselową dokładnością zamiast sztywnej wartości 100 kroków
    – Format wyjściowy wybrany w oknie przetwarzania wsadowego jest pamiętany
    – Unsharp masking nie zwalnia przy dużych wartościach „sigmy”
    – Zwiększony zakres parametrów unsharp maskingu
    
    Poprawki błędów:
    – Zła nazwa pliku wyjściowego, jeśli nazwa wejściowa zawierała więcej niż jedną kropkę
    – Błąd w momencie ręcznego wprowadzenia nieistniejącej ścieżki
    – Okna programu rozmieszczone poza ekranem, gdy poprzednio uruchomiono ImPPG w konfiguracji wieloekranowej
    – Przywrócono brakujące polskie napisy w interfejsie użytkownika

0.4 (2015-06-21)
    Nowe funkcje:
    – Wyrównywanie sekwencji poprzez stabilizację krawędzi tarczy słonecznej
    – Obsługa plików FITS (odczyt i zapis)
    – Zmiana powiększenia widoku
    
    Ulepszenia:
    – Przewijanie widoku przez przeciąganie środkowym przyciskiem myszy
    – Pamiętanie ustawienia wyświetlania histogramu w skali logarytmicznej

    Poprawki błędów:
    – Krzywa tonalna w trybie gamma nie jest aplikowana podczas przetwarzania wsadowego

0.3.1 (2015-03-22)
    Nowe funkcje:
    – Polska wersja językowa; dodano instrukcję tworzenia kolejnych tłumaczeń

0.3 (2015-03-19)
  Nowe funkcje:
    – Wyrównywanie sekwencji obrazów metodą korelacji fazowej
    
  Ulepszenia:
    – Ograniczona częstotliwość restartowania przetwarzania w trakcie edycji obrazu, w efekcie większa responsywność podczas zmiany parametrów unsharp maskingu i edycji krzywej tonalnej
    
  Poprawki błędów:
    – Niewłaściwe rozszerzenie plików wyjściowych po przetwarzaniu wsadowym, gdy wybrany format różni się od wejściowego

0.2 (2015-02-28)
  Nowe funkcje:
    – Obsługa większej liczby formatów plików graficznych poprzez FreeImage. Nowe formaty wyjściowe: PNG 8-bitowy, TIFF 8-bitowy z kompresją LZW, TIFF 16-bitowy z kompresją ZIP, TIFF 32-bit zmiennoprzecinkowy (bez kompresji lub z kompresją ZIP).
    
  Ulepszenia:
    – Uaktywniony „nowoczesny” styl elementów kontrolnych pod Windows

  Poprawki błędów:
    – Ramka zaznaczenia niewidoczna na platformach bez obsługi rastrowych operacji logicznych (np. GTK 3)

0.1.1 (2015-02-24)
  Poprawki błędów:
    – Puste pliki wynikowe po przetwarzaniu wsadowym przy zerowej liczbie iteracji L-R

0.1 (2015-02-21)
  Pierwsza wersja.
```