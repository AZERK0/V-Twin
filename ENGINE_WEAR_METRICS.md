# Écran de condition moteur et métriques d'usure

## 1. Objet

Ce document décrit le nouvel écran `ENGINE CONDITION`, les valeurs qu'il affiche et les garanties associées à chacune d'elles.

L'écran est un tableau de suivi thermique et opérationnel. Il ne prétend plus calculer une santé globale, une durée de vie restante ou un dommage mécanique sans modèle calibré. Cette distinction est volontaire : une température issue d'un bilan thermique peut être présentée comme une estimation physique, tandis qu'un pourcentage d'usure sans données d'essai ne doit pas être présenté comme une mesure.

Le détail du réseau thermique, de ses équations et de sa calibration se trouve dans [`ENGINE_THERMAL_MODEL.md`](ENGINE_THERMAL_MODEL.md).

Fichiers principaux :

- `include/simulation/engine_condition_model.h` ;
- `src/simulation/engine_condition_model.cpp` ;
- `include/simulation/engine_thermal_model.h` ;
- `src/simulation/engine_thermal_model.cpp` ;
- `include/ui/engine_condition_cluster.h` ;
- `src/ui/engine_condition_cluster.cpp`.

## 2. Architecture

La fonctionnalité est séparée en trois responsabilités :

1. `EngineThermalModel` calcule les températures physiques de l'huile, des pistons et des cylindres.
2. `EngineConditionModel` construit un snapshot cohérent des états moteur, véhicule, transmission, banc et thermique.
3. `EngineConditionCluster` convertit les unités, compose les libellés et dessine l'écran sans inventer de donnée métier.

Les métriques communes ne possèdent pas de second rendu spécifique à cet écran. `RightGaugeCluster` et `LoadSimulationCluster`, déjà utilisés par l'écran principal, proposent un layout compact et réemploient les mêmes instances de `LabeledGauge`, leurs unités, leurs bandes colorées et leur amortissement d'aiguille. Le dashboard thermique ne dessine donc plus de copies textuelles de ces valeurs.

La représentation mécanique est également la même instance d'`EngineView` que dans les autres vues. Elle conserve le rendu animé du moteur, le déplacement par glisser-déposer, le zoom à la molette et la sélection de couche existante.

`EngineConditionState` est publié à la fin de chaque frame de simulation. Il contient des valeurs brutes dans les unités internes et une copie du dernier `EngineThermalState`. La révision du snapshot permet à l'interface de ne reformater les chaînes que lorsqu'une nouvelle valeur est disponible.

Cette structure permet d'ajouter plus tard une famille de métriques sans coupler son calcul au rendu. Un futur modèle d'usure devra publier son propre état validé ou étendre le snapshot de condition, mais ses équations resteront dans la couche simulation.

## 3. Navigation et organisation de l'écran

Le simulateur possède maintenant quatre vues accessibles avec `Tab` :

1. analyse moteur et oscilloscopes, avec le débit d'échappement au centre ;
2. vue moteur plein écran ;
3. vue moteur avec instrumentation latérale ;
4. écran `ENGINE CONDITION`.

La touche `J` bascule directement entre l'analyse moteur et l'écran de condition.

L'écran de condition est organisé dans l'ordre de lecture suivant :

- identité du moteur et nature des données ;
- états `Ignition`, `Starter`, `Dyno` et `Hold`, rapport, couple et puissance dans le composant compact du banc ;
- régime, vitesse, pression collecteur, débit d'air et rendement volumétrique dans les jauges compactes communes ;
- vue moteur 2D interactive à côté des trois grandes jauges thermiques ;
- carte thermique par cylindre ;
- point de fonctionnement instantané ;
- périmètre et limites du modèle.

## 4. Nature et provenance des données

| Groupe | Métrique UI | Source | Nature |
|---|---|---|---|
| États | `IGNITION` | `IgnitionModule::m_enabled` | état direct |
| États | `STARTER` | `StarterMotor::m_enabled` | état direct |
| États | `DYNO` | `Dynamometer::m_enabled` | état direct |
| États | `HOLD` | `Dynamometer::m_hold` | état direct |
| Cinématique | `ENGINE SPEED` | vitesse du vilebrequin | calcul d'unité |
| Cinématique | `VEHICLE SPEED` | `Vehicle::getSpeed()` | calcul d'unité |
| Transmission | `GEAR` | `Transmission::getGear()` | état direct |
| Banc | `TORQUE` | couple filtré du dynamomètre | signal simulé |
| Banc | `POWER` | couple du banc et vitesse moteur | signal dérivé |
| Admission | `MANIFOLD PRESSURE` | `Engine::getManifoldPressure()` | signal simulé |
| Admission | `AIR FLOW` | `Engine::getIntakeFlowRate()` | signal simulé |
| Admission | `VOLUMETRIC EFF.` | débit réel / débit théorique | signal dérivé |
| Commande | `THROTTLE` | `Engine::getThrottle()` | état direct |
| Combustion | `INTAKE AFR` | `Engine::getIntakeAfr()` | signal simulé |
| Combustion | `EXHAUST O2` | `Engine::getExhaustO2()` | signal simulé |
| Thermique | huile, pistons, cylindres | `EngineThermalState` | observateur physique V1 |
| Qualité | `THERMAL INPUT` | validité et anomalies du modèle thermique | diagnostic numérique |

Aucune de ces valeurs ne provient d'un générateur aléatoire. Les aiguilles ont une animation amortie pour la lisibilité, mais leur cible reste la valeur du snapshot ; cette animation ne modifie pas la donnée affichée.

## 5. Télémétrie commune

### 5.1 Régime moteur

La vitesse angulaire interne $\omega$ est convertie en tours par minute :

$$
N_{rpm}=\frac{60\omega}{2\pi}
$$

Unité UI : `rpm`.

### 5.2 Vitesse véhicule

La vitesse linéaire interne est convertie vers `mph` ou `kph` selon `ApplicationSettings::speedUnits`. La valeur absolue est affichée afin que la marche arrière ne produise pas une vitesse de cadran négative.

### 5.3 Rapport engagé

La transmission représente le point mort par `-1`. L'interface affiche donc :

$$
G_{UI}=\begin{cases}
\mathrm{N} & G=-1\\
G+1 & G\geq 0
\end{cases}
$$

### 5.4 Couple et puissance

Le couple affiché est `Simulator::getFilteredDynoTorque()`. Il est converti en `Nm` ou `lb-ft` selon la configuration.

La puissance du banc est calculée par le simulateur :

$$
P=\tau\omega
$$

Elle est affichée en `kW` ou `hp`. Quand le banc ne charge pas le moteur, le couple et la puissance peuvent légitimement être nuls. Le nouvel écran n'affiche pas automatiquement un ancien pic à la place de la valeur instantanée.

### 5.5 Pression d'admission

La pression collecteur est affichée en `kPa abs`. Une pression absolue évite l'ambiguïté entre dépression, pression atmosphérique et suralimentation :

$$
p_{kPa,abs}=\frac{p_{Pa}}{1000}
$$

### 5.6 Débit d'air

`AIR FLOW` convertit le débit d'admission interne vers le débit standard `SCFM` avec le système d'unités existant. Il s'agit du débit fourni par la simulation d'admission, pas d'une mesure de débitmètre.

### 5.7 Rendement volumétrique

Le rendement volumétrique compare le débit molaire réel au débit théorique d'un moteur quatre temps, à la pression et à la température de référence :

$$
\dot n_{th,rev}=\frac{1}{2}\frac{p_{ref}V_d}{RT_{ref}}
$$

$$
\dot n_{th}=\dot n_{th,rev}\frac{N_{rpm}}{60}
$$

$$
\eta_v=\frac{\dot n_{air}}{\dot n_{th}}
$$

avec :

- $p_{ref}=1\ \mathrm{atm}$ ;
- $T_{ref}=25\ ^\circ\mathrm{C}$ ;
- $V_d$ la cylindrée totale ;
- le facteur $1/2$ correspondant à une admission tous les deux tours.

À régime nul ou lorsque le débit théorique est numériquement trop faible, l'interface publie `0 %`. Une valeur supérieure à `100 %` reste possible pour un moteur suralimenté ou bénéficiant d'un bon remplissage dynamique.

## 6. Jauges thermiques

### 6.1 Température d'huile

`OIL TEMPERATURE` affiche $T_o$, la température moyenne du nœud global d'huile. Elle se rapproche d'une température moyenne de carter. Elle ne représente ni la température maximale du film au palier, ni celle du jet sous piston.

### 6.2 Température de piston

`PISTON TEMPERATURE` affiche :

$$
T_{p,max}=\max_i(T_{p,i})
$$

Chaque $T_{p,i}$ est une température moyenne concentrée du piston. La valeur n'est pas une température locale de calotte ou de gorge de segment.

### 6.3 Température de cylindre

`CYLINDER TEMPERATURE` affiche :

$$
T_{c,max}=\max_i(T_{c,i})
$$

Chaque $T_{c,i}$ représente une température moyenne de paroi/liner associée au cylindre. La culasse et le liquide de refroidissement ne sont pas des nœuds séparés en V1.

### 6.4 Bandes colorées

Les bandes donnent un repère de lecture cohérent, pas une limite constructeur ni une alarme certifiée.

| Jauge | Froid | Usuel indicatif | Attention | Élevé |
|---|---:|---:|---:|---:|
| Huile | 20–70 °C | 70–120 °C | 120–140 °C | 140–180 °C |
| Piston moyen | 20–120 °C | 120–300 °C | 300–360 °C | 360–450 °C |
| Cylindre moyen | 20–70 °C | 70–180 °C | 180–220 °C | 220–300 °C |

Ces seuils sont codés dans `EngineConditionCluster`. Ils devront devenir configurables par moteur avant tout usage de validation.

## 7. Carte thermique par cylindre

La carte affiche, pour chaque cylindre :

- `PISTON` : $T_{p,i}$ ;
- `CYLINDER` : $T_{c,i}$ ;
- `DELTA` : $T_{p,i}-T_{c,i}$.

Une différence de température en kelvins est numériquement identique à une différence en degrés Celsius. La ligne contenant le piston le plus chaud ou le cylindre le plus chaud est mise en évidence.

Jusqu'à six cylindres, la carte utilise une colonne. Au-delà, elle se répartit automatiquement sur deux colonnes. Le modèle thermique conserve un état pour tous les cylindres réellement chargés ; l'interface n'est donc pas limitée au V-Twin malgré le nom du projet.

## 8. Point de fonctionnement

`OPERATING POINT` complète les grandes valeurs avec quatre signaux utiles au diagnostic :

- ouverture du papillon en pourcentage ;
- AFR d'admission ;
- oxygène d'échappement en pourcentage ;
- validité des entrées thermiques.

`THERMAL INPUT` vaut :

- `VALID` si le snapshot thermique est valide et qu'aucune entrée n'a été bornée ;
- `N ANOMALIES` si le modèle a dû protéger un échantillon physique hors domaine ;
- `UNAVAILABLE` si le modèle thermique n'est pas initialisé.

Le compteur d'anomalies est un diagnostic numérique. Il ne doit pas être interprété comme un nombre de défaillances moteur.

## 9. Valeurs initiales et absence de fausses données dynamiques

Au reset, `EngineConditionState` utilise uniquement des valeurs neutres et fixes :

- snapshot invalide ;
- états système désactivés ;
- régime, vitesse, couple, puissance, pression, débit et rendement à zéro ;
- rapport au point mort ;
- état thermique invalide.

Dès qu'un moteur est chargé et qu'une frame est simulée, ces valeurs sont remplacées par les signaux réels. Si le modèle thermique n'est pas disponible, les jauges affichent `NO DATA` au lieu de fabriquer une température.

Les anciens calculs dynamiques de `Health`, `RUL`, `Damage Rate`, réserves, modes de défaillance et dommages cumulés ont été supprimés. Le panneau `MODEL COVERAGE` affiche explicitement `WEAR / RUL: NOT CALIBRATED`.

## 10. Limites actuelles

Les seules masses solides thermiquement simulées sont les pistons et les parois de cylindre, avec un nœud global d'huile. Il n'existe actuellement aucun nœud distinct pour :

- le liquide de refroidissement ;
- la culasse ;
- les soupapes et sièges ;
- les segments ;
- les coussinets et paliers ;
- le collecteur et l'échappement ;
- le carter d'huile en tant que masse séparée ;
- un radiateur ou échangeur d'huile explicite.

Le rejet par carter, air, liquide et éventuel échangeur est regroupé dans les conductances équivalentes du modèle thermique. Ce choix permet une V1 exploitable, mais ne permet pas de diagnostiquer quel élément du circuit de refroidissement limite le système.

Il n'existe pas non plus de pression d'huile, de débit d'huile, de viscosité dépendante de la température, de cliquetis mesuré ou d'historique persistant. Par conséquent, le projet ne calcule actuellement ni :

- usure de palier ;
- usure de segment ;
- fatigue de soupape ;
- dommage de joint de culasse ;
- durée de vie restante ;
- santé globale.

## 11. Conditions pour ajouter de vraies métriques d'usure

Une nouvelle métrique d'usure ne doit être exposée que si les éléments suivants sont définis :

1. pièce ou mécanisme physique concerné ;
2. signal d'entrée disponible et unité ;
3. équation, corrélation ou table de dommage traçable ;
4. paramètres configurables par moteur ;
5. domaine de validité ;
6. données d'essai utilisées pour la calibration ;
7. tests unitaires et tests de non-régression ;
8. niveau de confiance présenté à l'utilisateur.

Exemples d'évolutions pertinentes :

- modèle de viscosité de type ASTM D341 à partir de la température d'huile ;
- pression et débit d'huile pour une marge de lubrification ;
- cycle thermique persistant pour une étude de fatigue calibrée ;
- température de culasse et pression cylindre maximale pour un indicateur joint/culasse ;
- compteur d'overspeed fondé sur une limite moteur configurable ;
- dommage cumulé selon Palmgren-Miner uniquement avec une courbe S-N adaptée à la pièce.

## 12. Vérification

La suite de tests couvre actuellement :

- le rendement volumétrique à l'état de référence ;
- le rendement nul moteur arrêté ;
- le coefficient de Hohenberg ;
- l'équilibre à l'ambiante ;
- la conservation de l'énergie de frottement ;
- la symétrie entre cylindres identiques ;
- la validation des paramètres et de la stabilité numérique ;
- le comptage des entrées physiques protégées.

Pour valider l'écran manuellement :

1. charger un moteur ;
2. ouvrir `ENGINE CONDITION` avec `J` ;
3. vérifier que le régime, le rapport et les états système suivent immédiatement les commandes ;
4. activer le banc et vérifier couple et puissance ;
5. vérifier que le moteur 2D s'anime, se déplace à la souris et zoome à la molette ;
6. observer une chauffe progressive des trois familles de nœuds ;
7. comparer la carte thermique au nombre de cylindres chargé ;
8. vérifier qu'un modèle thermique absent affiche `NO DATA`.

## 13. Références

Les références scientifiques et normatives relatives au transfert thermique, aux corrélations de Hohenberg/Woschni, aux huiles et aux essais moteur sont regroupées dans la section 14 de [`ENGINE_THERMAL_MODEL.md`](ENGINE_THERMAL_MODEL.md). Pour un futur modèle d'endommagement, les références devront être spécifiques à la pièce, au matériau, au chargement et à la méthode de calibration retenus.
