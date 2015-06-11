# Introduction #

Cette page décrit le mode d'utilisation de Tamanoir pour dépoussiérer les scans de photos.

Elle présente les éléments de l'outil, les raccourcis et les astuces pour une utilisation efficace.

Le mode d'emploi est découpé en deux parties :
  * un cas d'utilisation pas à pas pour décrire comment utiliser Tamanoir dans l'esprit où il a été conçu
  * l'explication détaillée de tous les boutons et outils


# Interface #

Voir le ToDo (en anglais) pour les développements futurs.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Global.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Global.png)

L'interface de Tamanoir est divisée en plusieurs sections :
  * **Affichages des images** sur la partie haute. A gauche la vue originale, à droite la vue corrigée.
  * Boutons **Ouvrir/Sauver** en dessous à gauche
  * La **Vignette de l'image entière** qui contient l'incrustation de la zone en cours
  * Les **boutons d'action** pour valider ou refuser les corrections proposées
  * Les **options :** type de film, résolution, mode "confiance"

Chaque bouton ou affichage contient une aide contextuelle (qui s'affiche lorsque la souris reste un moment sur l'objet) décrivant sa fonction.


---


# Cas d'utilisation typique / Tutoriel pas à pas #

Tamanoir a été conçu pour éviter les manipulations trop longues de correction des poussières. Avec deux idées :
  1. le logiciel détecte automatiquement les poussières pour proposer une correction à l'utilisateur. Il corrige donc une poussière par copier/coller (ou clonage) tout en laissant le choix à l'utilisateur d'accepter ou non ,
  1. on donne à l'utilisateur un moyen simple pour corriger manuellement les poussières restantes (outil de clonage).

L'utilisation typique de Tamanoir est ainsi la suivante :
  1. On ouvre une image : sélectionner le fichier à nettoyer
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Ouvrir.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Ouvrir.png)
    * Attendre la fin du chargement. La progression est indiquée en dessous du bouton "Sauver"
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-OuvrirSauver.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-OuvrirSauver.png)

  * Vérifier si les options correspondent au type de film et à la résolution du scan
![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Options.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Options.png)

  1. Une première passe en mode assisté : le logiciel détecte les poussières et propose les corrections. L'utilisateur utilise les boutons ou les raccourcis claviers pour valider ('A' ou bouton "Appliquer") ou refuser les corrections (flèche droite ou bouton "Refuser")
> A chaque proposition, l'affichage détaillé présente **l'image originale à gauche et l'image corrigée à droite**, à l'échelle 1:1 ou 100%.
> ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Details.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Details.png)
> On peut alors :
    * **Modifier la correction proposée** en changeant au choix : la position de l'origine de la copie (le rectangle vert), ou sa destination (le bout de la flèche verte), ou sa taille (soit en étirant le rectangle vert à la souris, soit en utilisant la molette de la souris). L'image de droite est actualisée en temps réel.
> > ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Actions.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Actions.png)
    * **Accepter (valider) ou refuser la correction**. L'acceptation se fait à l'aide du bouton "Appliquer" ou de la touche 'A'. ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-A.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-A.jpg)
> > On peut passer à la correction suivante avec le bouton "Refuser" ou la touche flèche droite (suivant)     ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Right.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Right.jpg)
> > Si on est pasé trop vite à la poussière suivante, on peut toujours revenir sur la ou les corrections précédemment refusées en cliquant sur le bouton "<" ou sur la flèche gauche ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Left.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-Left.jpg)


> Une fois cette étape terminée, **la plupart des poussières a été supprimée.** Cependant, des poussières ont probablement été oubliées ou le logiciel n'avait pas de proposition de correction, il faut donc les corriger à la main. De plus, le logiciel est incapable de supprimer les fibres en raison de leur taille importante dans l'image et parce qu'il est difficile de trouver une zone de correction automatiquement.
> Il faut donc inspecter l'image manuellement.

  1. On effectue une seconde passe en mode manuel : on appuie sur la touche "Début" ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-home.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-home.jpg) , on corrige les fibres et poussières restantes dans la zone affichée en mode "Clonage" (raccourci : touche 'C'), puis on passe au bloc suivant en appuyant sur la touche "Page suivante". ![http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-PageDown.jpg](http://tamanoir.googlecode.com/svn/trunk/screenshots/Key-PageDown.jpg)
> Et ainsi de suite jusqu'au coin en bas à droite de l'image.
  1. On sauvegarde l'image. Eventuellement selon son organisation, on archive l'originale et/ou on tague la version corrigée.


---


# Détail des fonctionnalités #

## Ouvrir / Sauver ##

  * **Ouvrir :** lance une fenêtre de choix de fichier image, puis effectue les pré-traitements en fonction des options choisies. A la fni des pré-traitement l'interface propose la première correction.
> > La barre de progression indique la progression du chargement et des pré-traitements.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Ouvrir.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Ouvrir.png)

  * **Sauver:** sauvegarde l'image courante avec le **même nom que le fichier original**. De cette façon, si vous avez indexé votre fichier avec un outil de gestion de photothèque, il sera automatiquement modifié, pas besoin de le réimporter. Une copie de sauvegarde esr générée avec l'extension _-copie_ (par dédefaut pour le moment. Cette copie sera éventuellement optionnelle dans les versions futures).

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-OuvrirSauver.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-OuvrirSauver.png)


## Vue générale / Vignette de l'image entière ##

La vue générale affiche une vignette de l'image entière, avec l'incrustation de petits rectangles :
  * **Rectangles bleus** : le grand rectangle bleu indique la zone affichée sur la vue à l'échelle 1:1, le petit à l'intérieur la position de la poussière détectée.
  * **Rectangles verts** : indique où les corrections ont été validées par l'utilisateur (bouton "Appliquer") ou validées par le mode "confiance".
  * **Rectangles jaunes** : indique où les corrections ont été refusées par l'utilisateur (bouton "Refuser").

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-VueGenerale.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-VueGenerale.png)

**Navigation :** on peut déplacer la zone de zoom avec les touches :
  * touche "début" : revient en haut à gauche de l'image
  * touche "page suivante" : déplace le zoom vers le bas. Quand le zoom atteint le bas de l'image, il se décale vers la droite et revient en haut.
  * touche "page précédente" : déplace le zoom vers le haut. Quand le zoom atteint le haut de l'image, il se décale vers la gauche et revient en bas.

## Vues détaillées : avant/après ##

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Details.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Details.png)

Il y a deux vues :
  * En haut à gauche, une partie ("crop") de l'image originale, à l'échelle 1:1 (100%). La correction proposée est matérialisée par un rectangle et une flèche verts :
    * Le rectangle représente l'origine (la source) de la copie. On peut modifier sa taille à la souris en étirant le rectangle par un coin, ou en utilisant la molette.
    * la pointe de la flèche désigne la destination de la copie. On la déplace avec la souris en maintenant le bouton de gauche appuyé.


> Pour modifier la copie, le plus rapide est de déplacer la destination avec la souris et de régler la taille avec la molette.

> La partie gauche affiche de plus un bouton "Clonage" (_raccourci : touche 'C'_). Si le clonage est activé :
    * La flèche suit en permanence le curseur de la souris. Si aucun bouton n'est appuyé, le logiciel cherche la meilleure source pour la destination pointée par la souris.
    * Le clic gauche effectue la copie du rectangle vers la pointe de la flèche. Attention : l'annulation (Ctrl+Z ou Pomme+Z) ne fonctionne que si l'on ne déplace pas la zone de zoom !
    * Le clic droit positionne l'origine (source) de la copie
    * La molette sert à redimensionner le rectangle source

  * En haut à droite, la version corrigée de l'image, actualisée en temps réel.


## Actions : Retour au début/précédent/refuser/appliquer ##

Il y a 3 types d'actions :
  * **Bouton Refuser** / (_Raccourci : flèche vers la droite_) : on passe à la poussière suivante sans appliquer la correction proposée.
  * **Bouton Appliquer** / (_Raccourci : touche 'A'_) : on applique la correction proposée, puis on passe à la poussière suivante.
  * **Bouton "<"** (précédent) / (_Raccourci : flèche vers la gauche_) : on revient à la poussière précédente (sans appliquer la correction proposée). C'est très pratique pour revenir sur une poussière que l'on a refusé par erreur en allant trop vite.
  * **Bouton "|<"** (retour au début) / (_Raccourci : flèche Début_) : on revient à la première poussière en haut à gauche de la photo (sans appliquer la correction proposée). Attention : il faudra alors refuser de nouveau toutes les propositions déjà refusées.


![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Actions.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Actions.png)

_**Astuce :**_ pour être efficace, il convient d'utiliser les raccourcis clavier 'A' et flèche de droite. On passe rapidement sur les corrections en les refusant ou en les validant, et si une correction est refusée trop vite, on utilise la flèche gauche pour revenir sur la proposition.

## Options : Film, résolution ##

Les options sont :
  * **Type de film type :** parmi :
    * _Indéfinis_: les poussières sont détectées si elles sont plus sombres ou plus claires que le fond
    * _Positif_ soit diapos couleurs ou photo numérique : les poussières sont détectées seulement si elles sont plus sombres que le fond
    * _Négatif_ soit négatifs couleurs : les poussières sont détectées seulement si elles sont plus claires que le fond

Le films couleurs ou noir et blancs sont détectés automatiquement. Les paramètres de détection s'adaptent à la présence de grain.

  * **Résolution** : ce paramètre modifie la taille minimale de la poussière à détecter. Plus la résolution est élevée, plus la poussière doit être de taille importante.

![http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Options.png](http://tamanoir.googlecode.com/svn/trunk/screenshots/Tamanoir-FR-Options.png)


## Mode "confiance" ##

Le mode "confiance" permet d'accélérer la tâche de correction en validant automatiquement les corrections qui sont jugées comme très bonnes par le logiciel. Les corrections sont indiquées en vert sur la vue générale.


## Mode "Vide seul."=Surfaces vides seulement ##

Le mode "Vide seulement" permet d'éviter de devoir confirmer ou refuser les poussières dans les zones où il y a beaucoup de fausses détections (graviers, surfaces tachetées...). Les poussières ne sont présentées que si elles se situent dans un espace uniforme : ciel, mur ...


---


Commentaires et suggestions sont bienvenues !! Nous cherchons de nouveaux contributeurs.