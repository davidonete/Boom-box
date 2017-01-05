#include "Scenes/LoginScene.h"
#include "System/GameManager.h"

LoginScene::LoginScene() {}

LoginScene::~LoginScene() 
{
    Window->RemoveAll();
}

void LoginScene::Init() 
{
    RenderWindow = GameManager::GetInstance()->GetWindow();

    sf::Texture::getMaximumSize();
    sf::Shader::isAvailable();

    sfg::Renderer::Set(sfg::VertexArrayRenderer::Create());

    RenderWindow->resetGLStates();

    //GUI
    InitGUI();

    //Background
    background.loadFromFile(GameManager::GetImagePath("background.jpg"));
    sprite.setTexture(background);

    sf::Vector2u winSize = RenderWindow->getSize();
    sf::Vector2f bgScale;
    bgScale.x = (winSize.x * 0.5f) / 800.0f;
    bgScale.y = (winSize.y * 1.0f) / 600.0f;
    sprite.setScale(bgScale);
}

void LoginScene::InitGUI()
{
    Window = sfg::Window::Create(sfg::Window::BACKGROUND | sfg::Window::SHADOW);

    Username = sfg::Entry::Create();
    Username->SetMaximumLength(32);
    Username->SetRequisition(sf::Vector2f(100.0f, 0.0f));

    Password = sfg::Entry::Create();
    Password->HideText('*');
    Password->SetMaximumLength(32);
    Password->SetRequisition(sf::Vector2f(100.0f, 0.0f));

    sfg::Button::Ptr LogIn = sfg::Button::Create("Log In");
    LogIn->SetRequisition(sf::Vector2f(100.0f, 5.0f));
    LogIn->GetSignal(sfg::Widget::OnLeftClick).Connect(std::bind(&LoginScene::OnLoginPressed, this));

    sfg::Button::Ptr Exit = sfg::Button::Create("Exit");
    Exit->SetRequisition(sf::Vector2f(100.0f, 5.0f));
    Exit->GetSignal(sfg::Widget::OnLeftClick).Connect(std::bind(&LoginScene::OnExitPressed, this));

    ErrorMsg = sfg::Label::Create("Log In:");
    ErrorMsg->SetId("ErrorMsg");
    
    sfg::Table::Ptr Table = sfg::Table::Create();
    Table->Attach(ErrorMsg, sf::Rect<sf::Uint32>(0, 0, 2, 1), sfg::Table::FILL, sfg::Table::FILL);
    Table->Attach(sfg::Label::Create(L"Username:"), sf::Rect<sf::Uint32>(0, 1, 1, 1), sfg::Table::FILL, sfg::Table::FILL);
    Table->Attach(Username, sf::Rect<sf::Uint32>(1, 1, 1, 1), sfg::Table::EXPAND | sfg::Table::FILL, sfg::Table::FILL);
    Table->Attach(sfg::Label::Create(L"Password:"), sf::Rect<sf::Uint32>(0, 2, 1, 1), sfg::Table::FILL, sfg::Table::FILL);
    Table->Attach(Password, sf::Rect<sf::Uint32>(1, 2, 1, 1), sfg::Table::FILL, sfg::Table::FILL);
    Table->Attach(LogIn, sf::Rect<sf::Uint32>(0, 3, 1, 1), sfg::Table::FILL, sfg::Table::FILL, sf::Vector2f(0.0f, 0.0f));
    Table->Attach(Exit, sf::Rect<sf::Uint32>(1, 3, 1, 1), sfg::Table::FILL, sfg::Table::FILL, sf::Vector2f(0.0f, 0.0f));
    Table->SetRowSpacings(5.f);
    Table->SetColumnSpacings(5.f);

    // Pack into box
    sfg::Box::Ptr Box = sfg::Box::Create(sfg::Box::Orientation::HORIZONTAL);
    Box->Pack(Table, true);
    Box->SetSpacing(5.0f);

    // Add box to the window
    Window->Add(Box);

    sf::Vector2f loginSize = Window->GetRequisition();
    sf::Vector2u winSize = RenderWindow->getSize();
    sf::Vector2f pos = sf::Vector2f((winSize.x * 0.5f) - (loginSize.x * 0.5f), (winSize.y * 0.5f) - (loginSize.y * 0.5f));
    Window->SetPosition(pos);

    Window->Update(0.0f);
    Username->GrabFocus();
}

void LoginScene::Input() {}

void LoginScene::Update() 
{
    sf::Event event;
    while (RenderWindow->pollEvent(event))
    {
        Window->HandleEvent(event);
        if (event.type == sf::Event::Closed)
            GameManager::GetInstance()->CloseGame();

        else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Tab)
        {
            if (Username->HasFocus())
                Password->GrabFocus();
            else
                Username->GrabFocus();
        }
        else if (event.type == sf::Event::KeyReleased && event.key.code == sf::Keyboard::Return)
            OnLoginPressed();
    }
    
    auto microseconds = GUIClock.getElapsedTime().asMicroseconds();

    // Only update every 5ms
    if (microseconds > 5000)
    {
        Window->Update(static_cast<float>(microseconds) / 1000000.f);
        GUIClock.restart();
    }
}

void LoginScene::Render() 
{
    RenderWindow->draw(sprite);
    GUI.Display(*RenderWindow);

    if (deleteSceneRequest)
        GameManager::GetInstance()->ChangeScene(GameScene_WaitRoom);
}

void LoginScene::OnLoginPressed()
{
    if (Username->GetText().isEmpty() || Password->GetText().isEmpty())
        LoginError("Wrong username or password. Try again.");
    else
    {
        GameManager* GM = GameManager::GetInstance();
        if (GM->Network->Connect(TCP))
        {
            LogInPacket packet;
            std::string username = Username->GetText().toAnsiString();
            std::string password = Password->GetText().toAnsiString();

            std::strcpy(packet.username, username.c_str());
            std::strcpy(packet.password, password.c_str());

            GM->Network->SendPacket(packet);

            ConfirmPacket confirmation;
            char buffer[128];
            GM->Network->ReceivePacket(TCP, buffer);
            if (GM->Network->GetPacketFromBytes(buffer, confirmation))
            {
                ConnectionMessage message = NetworkManager::GetConnectionMessage(confirmation.msg);
                if (message == Connection_Accepted || message == Connection_Accepted_Authority)
                {
                    if (message == Connection_Accepted_Authority)
                        GM->Network->SetAuthority(true);

                    GM->Network->SetClientID(confirmation.id);
                    GM->Network->SetUsername(username);
                    deleteSceneRequest = true;
                }
                else if (message == Connection_AlreadyLogged)
                    LoginError("This user is already logged in the game.");
                else
                    LoginError("Wrong username or password. Try again.");
            }
            else
                LoginError("Error receiving confirmation. Try again later.");
        }
        else
            LoginError("Error conecting to the server. Try again later.");
    }
}

void LoginScene::OnExitPressed()
{
    GameManager::GetInstance()->CloseGame();
}

void LoginScene::LoginError(sf::String message)
{
    sfg::Context::Get().GetEngine().SetProperty("Label#ErrorMsg", "Color", sf::Color(127, 0, 0, 255));
    ErrorMsg->SetText(message);
}
