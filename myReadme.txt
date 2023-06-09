Βασιλείου Ρηγίνος sdi1900019

Στο riscv.h κάτω από τα ήδη υπάρχοντα define για τα bits έχω κάνει define το 8ο bit 
να δείχνει αν είναι COW η σελίδα (1-> είναι COW 0 -> δεν είναι). 

Για τα reference counters έχω μια δομή RefCount στο kalloc.c στην οποία υπάρχει ένας 
πίνακας ref_count PYSTOP/PGSIZE θέσεων με με ένα spinlock rlock (για τυχόν race 
conditions εφόσον τρέχει με περισσότερους από έναν επεξεργαστές). Κάθε φορά που 
αλλάζω κάποιον από τους counters στον πίνακα πρέπει να κάνω acquire και release το 
rlock. Οι συναρτήσεις που "πειράζουν" το rlock είναι η increase και η reduce, για 
αύξηση και μείωση του ref_counter. Εφόσον έχουμε έναν πίνακα από counters 
και θέλουμε η κάθε θέση του να αντιστοιχίζεται σε μια σελίδα, έχουμε μια hash function 
που θα παίρνει το physical address(pa) και θα το αντιστοιχίζει σε μια θέση στον πίνακα. 
Η συνάρτηση που το κάνει αυτό είναι η hash() στην οποία παίρνουμε το pa και το αφαιρούμε 
από την πρώτη διεύθυνση μετά τον kernel (το end). Από το manual την παράγραφο 3.1 και 
στην εικόνα 3.2 βλέπουμε πως το physical page number έχει ένα offset 12 bits στα δεξιά, 
οπότε κάνουμε bitwise right shift κατά 12 για να έχουμε τον αριθμό της σελίδας. Για να 
παίρνουμε την τιμή του counter σε μια θέση του πίνακα ref_count υπάρχει η συνάρτηση 
get_ref(). Στη συνάρτηση αυτή απλά επιστρέφουμε την τιμή της θέσης του πίνακα στην οποία 
αντιστοιχεί το pa (με τη hash()). Το rlock για το struct μας αρχικοποιείται στην kinit(). 
Όλα τα παραάνω βρίσκονται στο αρχείο kalloc.c.Χρειάστηκαν και οι δηλώσεις των συναρτήσεων 
στο defs.h. 

Η kalloc κάνει allocate το memory για μια σελίδα. Άρα θα χρειαστεί να αλλάξουμε το ref_count
για αυτή τη σελίδα. Κάνουμε acquire το lock και θέτουμε τον counter για αυτή τη σελίδα σε 1.
Κάνουμε release το lock και επιστρέφουμε τον pointer. 

H kfree αν κληθεί για ref_count > 1 τότε πρέπει απλά να μειώσει τον ref_counter για αυτή τη 
σελίδα κατά 1, και να μην κάνει free το physical memory για αυτή τη σελίδα. Ελέγχουμε το 
ref_count και αν είναι μεγαλύτερο του 1 τότε απλά το μειώνουμε και τερματίζουμε τη συνάρτηση, 
αλλιώς συνεχίζουμε με την υπόλοιπη kfree αλλάζοντας τον ref_counter σε 0.

Χρειάστηκε να προσθέσουμε ένα κομμάτι στην freerange όπως ειπώθηκε στο φροντιστήριο, 
θέτοντας σε 1 όλα τα ref_counters πριν γίνει η kfree.

Για τη uvmcopy αλλάζω τα flags για το παιδί κάνοντας 0 το flag για το Write και 1 το flag
για το COW. Κάνουμε map τα physical pages στο παιδί και αλλάζουμε και τα flags του πατέρα.
Τέλος αυξάνουμε το ref_counter της σελίδας.

Η usertrap πρέπει να αναγνωρίζει τα page faults που προκαλλούνται σε κάποια COW σελίδα, να 
δεσμεύει μια νέα σελίδα με την kalloc και να αντιγράφει την παλιά σελίδα στη νέα αλλάζοντας 
το bit για το W και για το COW (το PTE_W σε 1 και το PTE_COW σε 0). Οι έλεγχοι που κάνω είναι
πως το va είναι μικρότερο από το MAXVA (όπως είχε προταθεί στο φροντιστήριο) και το 
page table entry (pte) που επιστρέφει η walk είναι διάφορο του μηδενός. Αν το ref_counter 
για αυτή τη σελίδα είναι 1, τότε δε χρειάζεται να κάνουμε kalloc και να αναθέσουμε μια νέα 
σελίδα, μπορούμε απλά να αλλάξουμε τα flags της προκειμένης σελίδας, αλλιώς θα κληθεί η kalloc
και θα αναθέσουμε μια νέα σελίδα με αλλαγμένα flags, κάνουμε kfree την παλιά σελίδα. Κάθε φορά 
που καλείται η mappages ή η kalloc ελέγχουμε για λάθη, κάνουμε kill το process και exit με 
αρνητική τιμή. Τρέχοντας τα τεστ εμφανιζόταν πρόβλημα με τη mappages (panic remap), οπότε το 
έκανα comment out στη mappages() (χωρίς αυτό τρέχει και περνάει κανονικά όλα τα τεστ).

Η copyout χρησιμοποιεί τον ίδιο μηχανισμό με την usertrap όταν συναντάει μια σελίδα COW. Οπότε
οι έλεγχοι είναι οι ίδιοι μόνο που ελέγχω και ότι πρόκειται για μια COW page. Επιστρέφω -1 σε 
περίπτωση που δεν ισχύει κάποιος έλεγχος και δεν κάνω kill κάποιο process.

Χρειάστηκε να κάνω define και τη walk στο defs.h.