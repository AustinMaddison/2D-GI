#include "raylib.h"

#ifndef GUI_PROGRESS_BAR_H
#define GUI_PROGRESS_BAR_H


typedef struct {

    // Anchors for panels
    Vector2 anchorFile;
    Vector2 anchorEdit;
    Vector2 anchorTools;
    Vector2 anchorVisuals;
    Vector2 anchorRight;
    
    // File options
    bool btnNewFilePressed;
    bool btnLoadFilePressed;
    bool btnSaveFilePressed;
    bool btnExportFilePressed;
    bool btnCloseFilePressed;

    // Editor options
    bool btnUndoPressed;                // Undo last action recorded
    bool btnRedoPressed;                // Redo last action recorded
    bool snapModeActive;                // Toggle snap to grid mode
    //bool editionModeActive;             // Toggle control creation/selection mode

    // Selected control options
    bool btnEditTextPressed;            // Edit control text
    bool btnEditNamePressed;            // Edit control name
    bool btnDuplicateControlPressed;    // Duplicate control
    bool btnUnlinkControlPressed;       // Unlink control from anchor

    bool btnDeleteControlPressed;       // Delete control

    // Selected anchor options
    bool btnEditAnchorNamePressed;      // Edit anchor name
    bool hideAnchorControlsActive;      // Toggle Hide/Show anchor linked controls
    bool btnUnlinkAnchorControlsPressed;   // Unlink all controls linked to selected anchor
    bool btnDeleteAnchorPressed;        // Delete selected anchor

    // Selected tracemap options
    bool btnLoadTracemapPressed;        // Load tracemap image
    bool hideTracemapActive;            // Hide/show tracemap image
    bool lockTracemapActive;            // Lock/unlock tracemap image
    bool btnDeleteTracemapPressed;      // Unload tracemap image
    float tracemapAlphaValue;           // Adjust tracemap opacity

    // Visual options
    bool showControlRecsActive;         // Toggle all controls rectangles drawing
    bool showControlNamesActive;        // Toggle all controls names
    bool showControlOrderActive;        // Toggle all control drawing order (layers)
    bool showControlPanelActive;        // Toggle control panel window
    bool showGridActive;                // Show/hide work grid
    bool showTooltips;                  // Show controls tooltips on mouse hover

    int visualStyleActive;
    int prevVisualStyleActive;
    int languageActive;

    // Info options
    bool btnHelpPressed;
    bool btnAboutPressed;
    bool btnIssuePressed;

    // Custom variables
    // NOTE: Required to enable/disable some toolbar elements
    int controlSelected;
    int anchorSelected;
    bool tracemapLoaded;

} GuiMainToolbarState;
