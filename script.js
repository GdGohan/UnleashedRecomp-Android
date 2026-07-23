/*
    CONFIGURAÇÃO DO REPOSITÓRIO

    Você pode usar a URL:

    index.html?owner=facebook&repo=react

    Ou:

    index.html?owner=torvalds&repo=linux

*/


const API_BASE =

    "https://api.github.com";


const params =

    new URLSearchParams(
        window.location.search
    );


const owner =

    params.get("owner")

    ||

    "GdGohan";


const repo =

    params.get("repo")

    ||

    "UnleashedRecomp-Android";


const currentPath =

    params.get("path")

    ||

    "";


let currentBranch =

    "main";


let repositoryData =

    null;


let currentFiles =

    [];
	

const CACHE_TIME = 5 * 60 * 1000;

function getCache(key) {
    const cached = localStorage.getItem(key);

    if (!cached) {
        return null;
    }

    const data = JSON.parse(cached);

    if (Date.now() - data.timestamp > CACHE_TIME) {
        localStorage.removeItem(key);
        return null;
    }

    return data.value;
}

function setCache(key, value) {
    localStorage.setItem(
        key,
        JSON.stringify({
            timestamp: Date.now(),
            value
        })
    );
}


/*
    ELEMENTOS DA PÁGINA
*/


const repositoryName =

    document.getElementById(
        "repositoryName"
    );


const repositoryDescription =

    document.getElementById(
        "repositoryDescription"
    );


const repositoryActions =

    document.getElementById(
        "repositoryActions"
    );


const fileList =

    document.getElementById(
        "fileList"
    );


const readmeContent =

    document.getElementById(
        "readmeContent"
    );


const searchInput =

    document.getElementById(
        "searchInput"
    );


const breadcrumb =

    document.getElementById(
        "breadcrumb"
    );


const refreshButton =

    document.getElementById(
        "refreshButton"
    );


const codeButton =

    document.getElementById(
        "codeButton"
    );


const toast =

    document.getElementById(
        "toast"
    );


/*
    INICIALIZAÇÃO
*/


document.addEventListener("DOMContentLoaded", async () => {
    setupTabs();
    setupSearch();
    setupButtons();

    await loadRepository();

    await Promise.all([
        loadFiles(currentPath),
        loadReadme(),
        loadLastCommit(),
        loadReleases()
    ]);
});


/*
    CARREGAR REPOSITÓRIO
*/


async function loadRepository() {

    try {

        const response = await fetch(
            `${API_BASE}/repos/${owner}/${repo}`
        );


        if (response.status === 404) {

            throw new Error(
                "Repository not found"
            );

        }


        if (response.status === 403) {

            throw new Error(
                "GitHub API rate limit exceeded. Please try again later."
            );

        }


        if (!response.ok) {

            throw new Error(
                `GitHub API error: ${response.status}`
            );

        }


        const repository =
            await response.json();


        document.getElementById(
            "repositoryName"
        ).textContent =
            repository.full_name;


        document.getElementById(
            "repositoryDescription"
        ).textContent =
            repository.description ||
            "No description available.";


        document.getElementById(
            "issuesCount"
        ).textContent =
            repository.open_issues_count;
			
	    
		repositoryData = repository;

		currentBranch = repository.default_branch || "main";

		updateRepositoryHeader();

		renderBreadcrumb();


    }

    catch (error) {

        console.error(error);


        document.getElementById(
            "repositoryName"
        ).textContent =
            "Unable to load repository";


        document.getElementById(
            "repositoryDescription"
        ).textContent =
            error.message;

    }

}


/*
    ATUALIZAR CABEÇALHO
*/


function updateRepositoryHeader() {


    document.title =

        `${repositoryData.full_name} - CodeSpace`;


    repositoryName.innerHTML =

        `

            ${escapeHTML(
                repositoryData.owner.login
            )}

            <span>/</span>

            ${escapeHTML(
                repositoryData.name
            )}

        `;


    repositoryDescription.textContent =

        repositoryData.description

        ||

        "Este repositório não possui descrição.";


    document.getElementById(

        "branchButton"

    ).textContent =

        `⎇ ${currentBranch}`;


    repositoryActions.innerHTML =

        `

        <button
            class="action-button"
            id="watchButton"
        >

            ◉ Acompanhar

            <span>

                ${repositoryData.subscribers_count}

            </span>

        </button>


        <button
            class="action-button"
            id="starButton"
        >

            ☆ Estrela

            <span>

                ${repositoryData.stargazers_count}

            </span>

        </button>


        <button
            class="action-button"
        >

            Fork

            <span>

                ${repositoryData.forks_count}

            </span>

        </button>

        `;


    document.getElementById(

        "starButton"

    ).addEventListener(

        "click",

        () => {

            showToast(
                "Estrelas são gerenciadas pelo GitHub."
            );

        }

    );


    document.getElementById(

        "watchButton"

    ).addEventListener(

        "click",

        () => {

            showToast(
                "Acompanhamento gerenciado pelo GitHub."
            );

        }

    );

}


/*
    CARREGAR ARQUIVOS
*/


async function loadFiles(path = "") {

    const url =
        `${API_BASE}/repos/${owner}/${repo}/contents/${path}`;


    const response =
        await fetch(url);


    if (!response.ok) {

        throw new Error(
            `Failed to load files: ${response.status}`
        );

    }


    const files =
        await response.json();


    renderFiles(files);

}


/*
    RENDERIZAR ARQUIVOS
*/


function renderFiles(files) {
	
	
	currentFiles = Array.isArray(files) ? files : [];
	

    fileList.innerHTML = "";


    if (!Array.isArray(files)) {


        fileList.innerHTML =

            `

            <div class="loading">

                Não foi possível listar os arquivos.

            </div>

            `;


        return;

    }


    const sortedFiles =

        [

            ...files

        ].sort(

            (a, b) => {

                if (

                    a.type === "dir"

                    &&

                    b.type !== "dir"

                ) {

                    return -1;

                }


                if (

                    a.type !== "dir"

                    &&

                    b.type === "dir"

                ) {

                    return 1;

                }


                return a.name.localeCompare(
                    b.name
                );

            }

        );


    sortedFiles.forEach(

        file => {


            const row =

                document.createElement(
                    "div"
                );


            row.className =

                "file-row";


            row.dataset.name =

                file.name.toLowerCase();


            const isFolder =

                file.type === "dir";


            row.innerHTML =

                `

                <div class="file-name">

                    <span
                        class="file-icon
                        ${isFolder ? "folder" : ""}"
                    >

                        ${isFolder ? "▰" : "▤"}

                    </span>


                    <strong>

                        ${escapeHTML(
                            file.name
                        )}

                    </strong>

                </div>


                <div class="file-description">

                    ${isFolder
                        ? "Pasta"
                        : "Arquivo"}

                </div>


                <div class="file-date">

                    ${formatFileType(
                        file
                    )}

                </div>

                `;


            row.addEventListener(

                "click",

                () => {

                    openFile(
                        file
                    );

                }

            );


            fileList.appendChild(
                row
            );

        }

    );

}


/*
    ABRIR ARQUIVO OU PASTA
*/


function openFile(file) {


    if (

        file.type === "dir"

    ) {


        const newUrl =

            `?owner=${encodeURIComponent(owner)}` +

            `&repo=${encodeURIComponent(repo)}` +

            `&path=${encodeURIComponent(file.path)}`;


        window.location.href =

            newUrl;


        return;

    }


    if (

        file.type === "file"

    ) {


        window.open(

            file.html_url,

            "_blank"

        );

    }

}


/*
    CAMINHO DE NAVEGAÇÃO
*/


function renderBreadcrumb() {


    breadcrumb.innerHTML = "";


    const home =

        document.createElement(
            "span"
        );


    home.textContent =

        `${owner}/${repo}`;


    home.style.cursor =

        "pointer";


    home.addEventListener(

        "click",

        () => {

            window.location.href =

                `?owner=${owner}&repo=${repo}`;

        }

    );


    breadcrumb.appendChild(
        home
    );


    if (!currentPath) {

        return;

    }


    const parts =

        currentPath.split("/");


    let accumulated = "";


    parts.forEach(

        part => {


            accumulated +=

                accumulated

                    ? `/${part}`

                    : part;


            const separator =

                document.createElement(
                    "span"
                );


            separator.textContent =

                " / ";


            breadcrumb.appendChild(
                separator
            );


            const item =

                document.createElement(
                    "span"
                );


            item.textContent =

                part;


            item.style.cursor =

                "pointer";


            const pathForLink =

                accumulated;


            item.addEventListener(

                "click",

                () => {


                    window.location.href =

                        `?owner=${owner}` +

                        `&repo=${repo}` +

                        `&path=${encodeURIComponent(
                            pathForLink
                        )}`;

                }

            );


            breadcrumb.appendChild(
                item
            );

        }

    );

}


/*
    CARREGAR README
*/


async function loadReadme() {


    const url =

        `${API_BASE}/repos/` +

        `${owner}/${repo}/readme`;


    const response =

        await fetch(url);


    if (!response.ok) {


        readmeContent.innerHTML =

            `

            <h1>
                README.md
            </h1>


            <p>

                Este repositório não possui README.md.

            </p>

            `;


        return;

    }


    const readme =

        await response.json();


    const content =

        decodeBase64(
            readme.content
        );


    readmeContent.innerHTML =

        markdownToHTML(
            content
        );

}


/*
    CARREGAR ÚLTIMO COMMIT
*/


async function loadLastCommit() {


    const url =

        `${API_BASE}/repos/` +

        `${owner}/${repo}/commits` +

        `?sha=${currentBranch}` +

        `&per_page=1`;


    const response =

        await fetch(url);


    if (!response.ok) {

        return;

    }


    const commits =

        await response.json();


    if (

        !commits.length

    ) {

        return;

    }


    const commit =

        commits[0];


    document.getElementById(

        "lastCommitAuthor"

    ).textContent =

        commit.commit.author.name;


    document.getElementById(

        "lastCommitMessage"

    ).textContent =

        commit.commit.message;


    document.getElementById(

        "lastCommitDate"

    ).textContent =

        formatDate(

            commit.commit.author.date

        );

}


/*
    PESQUISA DE ARQUIVOS
*/


function setupSearch() {


    searchInput.addEventListener(

        "input",

        () => {


            const term =

                searchInput.value

                    .toLowerCase()

                    .trim();


            const filtered =

                currentFiles.filter(

                    file =>

                        file.name

                            .toLowerCase()

                            .includes(term)

                );


            renderFiles(
                filtered
            );

        }

    );

}


/*
    ABAS
*/


function setupTabs() {


    const tabs =

        document.querySelectorAll(
            ".tab"
        );


    const contents =

        document.querySelectorAll(
            ".tab-content"
        );


    tabs.forEach(

        tab => {


            tab.addEventListener(

                "click",

                () => {


                    const target =

                        tab.dataset.tab;


                    tabs.forEach(

                        item => {

                            item.classList.remove(
                                "active"
                            );

                        }

                    );


                    contents.forEach(

                        content => {

                            content.classList.remove(
                                "active"
                            );

                        }

                    );


                    tab.classList.add(
                        "active"
                    );


                    const selected =

                        document.getElementById(

                            `${target}Tab`

                        );


                    if (selected) {

                        selected.classList.add(
                            "active"
                        );

                    }

                }

            );

        }

    );

}


/*
    BOTÕES
*/


function setupButtons() {


    refreshButton.addEventListener(

        "click",

        () => {

            loadRepository();
			
			loadReleases();


            showToast(
                "Repositório atualizado!"
            );

        }

    );


    codeButton.addEventListener(

        "click",

        () => {

            showToast(

                "O código deste projeto está disponível no GitHub."

            );

        }

    );

}


/*
    BASE64 UTF-8
*/


function decodeBase64(content) {


    const clean =

        content.replace(
            /\n/g,
            ""
        );


    const binary =

        atob(clean);


    const bytes =

        Uint8Array.from(

            binary,

            character =>

                character.charCodeAt(0)

        );


    return new TextDecoder(
        "utf-8"
    ).decode(bytes);

}


async function loadReleases() {

    const releasesList =
        document.getElementById(
            "releasesList"
        );


    const viewAllReleases =
        document.getElementById(
            "viewAllReleases"
        );


    if (!releasesList) {

        return;

    }


    try {


        const response =
            await fetch(

                `https://api.github.com/repos/${owner}/${repo}/releases`

            );


        if (!response.ok) {

            throw new Error(
                "Failed to load releases"
            );

        }


        const releases =
            await response.json();


        viewAllReleases.href =
            `https://github.com/${owner}/${repo}/releases`;


        if (
            releases.length === 0
        ) {

            releasesList.innerHTML = `

                <div class="empty-releases">

                    No releases published yet.

                </div>

            `;

            return;

        }


        releasesList.innerHTML =
            releases
                .slice(0, 5)
                .map(
                    release => {

                        const date =
                            new Date(
                                release.published_at ||
                                release.created_at
                            );


                        const formattedDate =
                            date.toLocaleDateString(
                                "en-US",
                                {

                                    year: "numeric",

                                    month: "short",

                                    day: "numeric"

                                }

                            );


                        const assets =
                            release.assets
                                ? release.assets.length
                                : 0;


                        return `

                            <article
                                class="release-card"
                            >


                                <div
                                    class="release-icon"
                                >

                                    ⬢

                                </div>


                                <div
                                    class="release-info"
                                >


                                    <div
                                        class="release-title-row"
                                    >


                                        <a
                                            href="${release.html_url}"
                                            target="_blank"
                                            rel="noopener noreferrer"
                                            class="release-title"
                                        >

                                            ${release.name || release.tag_name}

                                        </a>


                                        ${
                                            release.prerelease

                                                ? `

                                                    <span
                                                        class="release-badge pre-release"
                                                    >

                                                        Pre-release

                                                    </span>

                                                `

                                                : `

                                                    <span
                                                        class="release-badge latest-release"
                                                    >

                                                        Latest

                                                    </span>

                                                `
                                        }


                                    </div>


                                    <div
                                        class="release-meta"
                                    >

                                        ${release.tag_name}

                                        ·

                                        ${formattedDate}

                                        ·

                                        ${assets}

                                        ${
                                            assets === 1
                                                ? "asset"
                                                : "assets"
                                        }

                                    </div>


                                    <p
                                        class="release-description"
                                    >

                                        ${
                                            release.body
                                                ? release.body
                                                    .replace(
                                                        /[#*`]/g,
                                                        ""
                                                    )
                                                    .slice(
                                                        0,
                                                        250
                                                    )
                                                : "No release description."
                                        }

                                    </p>


                                </div>


                                <a
                                    href="${release.html_url}"
                                    target="_blank"
                                    rel="noopener noreferrer"
                                    class="release-button"
                                >

                                    View release

                                </a>


                            </article>

                        `;

                    }

                )
                .join("");


    }

    catch (error) {


        console.error(
            error
        );


        releasesList.innerHTML = `

            <div class="error-message">

                Unable to load releases.

            </div>

        `;

    }

}


/*
    MARKDOWN SIMPLES
*/


function markdownToHTML(markdown) {

    let html = marked.parse(markdown);


    /*
    ==========================================
    GITHUB ALERTS
    ==========================================
    */


    html = html.replace(

        /<blockquote>\s*<p>\[!IMPORTANT\]\s*([\s\S]*?)<\/p>\s*<\/blockquote>/gi,

        function(match, content) {

            return `

                <div class="markdown-alert markdown-alert-important">

                    <div class="markdown-alert-title">

                        IMPORTANT

                    </div>

                    <div class="markdown-alert-content">

                        ${content}

                    </div>

                </div>

            `;

        }

    );


    html = html.replace(

        /<blockquote>\s*<p>\[!WARNING\]\s*([\s\S]*?)<\/p>\s*<\/blockquote>/gi,

        function(match, content) {

            return `

                <div class="markdown-alert markdown-alert-warning">

                    <div class="markdown-alert-title">

                        WARNING

                    </div>

                    <div class="markdown-alert-content">

                        ${content}

                    </div>

                </div>

            `;

        }

    );


    html = html.replace(

        /<blockquote>\s*<p>\[!NOTE\]\s*([\s\S]*?)<\/p>\s*<\/blockquote>/gi,

        function(match, content) {

            return `

                <div class="markdown-alert markdown-alert-note">

                    <div class="markdown-alert-title">

                        NOTE

                    </div>

                    <div class="markdown-alert-content">

                        ${content}

                    </div>

                </div>

            `;

        }

    );


    return DOMPurify.sanitize(html);

}


/*
    SEGURANÇA BÁSICA
*/


function escapeHTML(text) {


    const div =

        document.createElement(
            "div"
        );


    div.textContent =

        text;


    return div.innerHTML;

}


/*
    FORMATAÇÕES
*/


function formatFileType(file) {


    if (

        file.type === "dir"

    ) {

        return "Pasta";

    }


    const extension =

        file.name.includes(".")

            ? file.name

                .split(".")

                .pop()

            : "arquivo";


    return extension.toUpperCase();

}


function formatDate(date) {


    return new Date(
        date
    ).toLocaleDateString(

        "pt-BR",

        {

            day: "2-digit",

            month: "2-digit",

            year: "numeric"

        }

    );

}


/*
    ESTADOS DA INTERFACE
*/


function showLoading() {


    fileList.innerHTML =

        `

        <div class="loading">

            Carregando arquivos...

        </div>

        `;


    readmeContent.innerHTML =

        `

        <div class="loading">

            Carregando README...

        </div>

        `;

}


function showError(message) {


    fileList.innerHTML =

        `

        <div class="empty-state">

            <h2>
                Erro
            </h2>


            <p>

                ${escapeHTML(
                    message
                )}

            </p>

        </div>

        `;

}


function showToast(message) {


    toast.textContent =

        message;


    toast.classList.add(
        "show"
    );


    setTimeout(

        () => {

            toast.classList.remove(
                "show"
            );

        },

        3000

    );

}
